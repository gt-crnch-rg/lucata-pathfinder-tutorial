#!/usr/bin/env python3

import os
import sys
import shutil
import itertools
import string
import argparse
import json
import textwrap
import re
import unicodedata

# https://stackoverflow.com/a/295466/2570605
def slugify(value, allow_unicode=False):
    """
    Convert to ASCII if 'allow_unicode' is False. Convert spaces to hyphens.
    Remove characters that aren't alphanumerics, underscores, or hyphens.
    Convert to lowercase. Also strip leading and trailing whitespace.
    """
    value = str(value)
    if allow_unicode:
        value = unicodedata.normalize('NFKC', value)
    else:
        value = unicodedata.normalize('NFKD', value).encode('ascii', 'ignore').decode('ascii')
    value = re.sub(r'[^\w\s-]', '', value).strip().lower()
    return re.sub(r'[-\s]+', '-', value)

class Args(dict):
    def __getattr__(self, key):
        return self[key]
    def __setattr__(self, key, value):
        self[key] = value
    def __str__(self):
        return ".".join(slugify(v) for v in list(self.values()))

def as_list(x):
    return x if type(x) is list else [x]

def iterSuite(suite):
    """
    Generates all the permutations of configuration parameters for a suite of experiments.

    'suite' should be a dict mapping strings to a list of values.
    On each iteration, returns an Args object with an attributed for each key set to one of the selected values
    """
    # Single suite
    if isinstance(suite, dict):
        keys = list(suite.keys())
        values = [as_list(v) for v in suite.values()]
        for args in itertools.product(*values):
            yield Args({k:v for k, v in zip(keys, args)})
    # List of suites
    elif isinstance(suite, list):
        for s in suite:
            for a in iterSuite(s):
                yield a

def check_local_config(local_config):
    """Make sure paths to input sets and executables are valid"""

    if local_config["platform"] in ["emu", "emuchick"]:
        return

    try:
        for benchmark, path in local_config["binaries"].items():
            if not os.path.isfile(path):
                raise Exception("Invalid path for {} executable: {}".format(benchmark, path))

        if local_config["platform"] not in ["native", "emusim", "emusim_profile", "emu", "emusim-chick-box", "emusim-validation"]:
            raise Exception("Platform {} not supported".format(local_config["platform"]))

    except KeyError as e:
        raise Exception("Missing {} field in local_config.json".format(e))


def check_args(args, local_config):
    pass

def pass_args(arg_names):
    """Generate a template for a command line"""
    return "\n".join("--{0} {{{0}}} \\".format(a) for a in arg_names)

def generate_script(args, script_dir, out_dir, local_config, no_redirect, no_algs):
    """Generate a script to run the experiment specified by the independent variables in args"""

    # Add platform name to args
    args.platform = local_config["platform"]
    script_name = os.path.join(script_dir, "{}.sh".format(args))

    # Derived params are calculated from 'args' when the script is generated
    args.update({
        "name" : str(args),
        # 'args' encoded as json
        "json" : json.dumps(args),
        # Path to the executable
        "exe" : local_config["binaries"][args.benchmark],
        # Results from performance hooks go here
        "outfile" : "{0}/{1}.txt".format(out_dir, str(args)),
        # Additional result files can be created here
        "outdir" : out_dir,
        # All other program output goes here
        "logfile" : "/dev/stderr" if no_redirect else "{0}/{1}.log".format(out_dir, str(args))
    })

    # Add params from local_config
    args.update(local_config)

    # Set slurm queue requirements
    template = """\
        #!/bin/bash -e
        #SBATCH --job-name={name}
        #SBATCH --output=/dev/null
        """
    # Set up output files
    template += """
        export LOGFILE="{logfile}"
        export OUTFILE="{outfile}"
        export HOOKS_FILENAME=$OUTFILE
        echo '{json}' | tee $LOGFILE $OUTFILE  >/dev/null
        echo `hostname` | tee -a $LOGFILE $OUTFILE >/dev/null
        """
    # Run multiple trials
    # template += """for trial in $(seq {num_trials}); do """

    # Emu hardware (single node) command line
    if local_config["platform"] == "native":
        template += """
        {exe} \\"""

    elif local_config["platform"] == "emu":
        template += """
        emu_handler_and_loader 0 0 {exe} -- \\"""

    # Emu hardware (multi node) command line
    elif local_config["platform"] == "emuchick":
        template += """
        emu_multinode_exec 0 --thread_quit_off {exe} -- \\"""

    # Emu profiler command line
    elif "emusim_profile" in local_config["platform"]:
        template += """
        {emusim_profile_exe}    \\
        {outdir}/profile.{name} \\
        {emusim_flags}          \\
        --                      \\
        {exe}                   \\"""

    # Emu simulator command line
    elif "emusim" in local_config["platform"]:
        template += """
        {emusim_exe} \\
        {emusim_flags} \\
        -o {outdir}/{name} \\
        -- {exe} \\"""

    if args.benchmark in ["local_stream", "global_stream", "global_stream_1d", "local_stream_cxx"]:
        # Generate the benchmark command line
        template += """
        {spawn_mode} {log2_num_elements} {num_threads} 1 \\
        &>> $LOGFILE
        """
    elif args.benchmark in ["global_stream_cxx"]:
        # Generate the benchmark command line
        template += """
        {spawn_mode} {layout} {log2_num_elements} {num_threads} 1 \\
        &>> $LOGFILE
        """
    elif args.benchmark == "pointer_chase":
        # Generate the benchmark command line
        template += """
        --log2_num_elements {log2_num_elements} \\
        --num_threads {num_threads} \\
        --block_size {block_size} \\
        --spawn_mode {spawn_mode} \\
        --sort_mode {sort_mode} \\
        --num_trials {num_trials} \\
        &>> $LOGFILE
        """

    else:
        raise Exception("Unsupported benchmark {}".format(args.benchmark))

    # template += "done\n"

    # Fill in the blanks
    command = textwrap.dedent(template).format(**args)

    # Write the script to file
    with open(script_name, "w") as f:
        f.write(command)
    os.system("chmod +x {}".format(script_name))

    # Return the path to the generated script
    return script_name

def generate_suite(suite, script_dir, out_dir, local_config, no_redirect, no_algs):
    """Generate a script for each permutation of parameters in the test suite"""

    script_names = []
    check_local_config(local_config)
    for args in iterSuite(suite):
        check_args(args, local_config)
        script_name = generate_script(args, script_dir, out_dir, local_config, no_redirect, no_algs)
        script_names.append(script_name)

    return script_names

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("platform", help="Hardware platform to generate scripts for")
    parser.add_argument("suite", help="Path to json file containing test suite definition")
    parser.add_argument("dir", help="Output directory for generated scripts and results")
    parser.add_argument("-f", "--force", default=False, action="store_true", help="Continue even if the results directory is not empty")
    parser.add_argument("--clean", default=False, action="store_true", help="Delete generated results before regenerating scripts")
    parser.add_argument("--no-redirect", default=False, action="store_true", help="Don't redirect output to file")
    parser.add_argument("--no-algs", default=False, action="store_true", help="Don't run any algorithms, just do incremental graph construction")
    args = parser.parse_args()

    # Prepare directories
    if not os.path.exists(args.dir):
        os.makedirs(args.dir)
    out_dir = os.path.join(args.dir, "results")
    script_dir = os.path.join(args.dir, "scripts")

    # Create output dir if it doesn't exist
    if not os.path.exists(out_dir):
        os.makedirs(out_dir)
    # Output dir not empty?
    elif os.listdir(out_dir) != []:
        # Clean out results
        if args.clean:
            # os.remove(os.path.join(args.dir, "launched"))
            shutil.rmtree(out_dir)
            os.makedirs(out_dir)
        # Just generate new ones over the top
        elif args.force:
            pass
        # Quit and complain
        else:
            sys.stderr.write("Found existing results in {}, pass --clean to remove them\n".format(out_dir))
            sys.exit(-1)

    # Create script dir if it doesn't exist
    if not os.path.exists(script_dir):
        os.makedirs(script_dir)
    # Script dir not empty?
    elif os.listdir(script_dir) != []:
        # Clean out generated scripts
        shutil.rmtree(script_dir)
        os.makedirs(script_dir)

    # Load the suite definition
    with open(args.suite) as f:
        suite = json.load(f)

    # Load paths to benchmarks/inputs for this machine
    with open("local_config.json") as f:
        local_config = json.load(f)[args.platform]
        local_config["platform"] = args.platform
        check_local_config(local_config)

    # Generate the scripts
    script_names = generate_suite(
        suite=suite,
        script_dir=script_dir,
        out_dir=out_dir,
        local_config=local_config,
        no_redirect=args.no_redirect,
        no_algs=args.no_algs
    )

    # Write paths to all generated scripts to a file
    joblist_file = os.path.join(args.dir, "joblist")
    with open(joblist_file, "w") as f:
        for script_name in script_names:
            f.write(script_name + "\n")

if __name__ == "__main__":
    main()
