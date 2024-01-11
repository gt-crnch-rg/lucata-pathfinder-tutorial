#!/usr/bin/env python3 

import os 
import argparse 
import subprocess 
import json 
import re 
'''
Purpose: Create dictionary that can be the input .hpc file to "make_hpc_plots.py" and
"compare_hwperfcntr_jsons.py" (combine dictionaries from each node into master dictionary)

mn_exec_pc.PID.log -> pc_concat.py -> mn_exec_pc_PID.hpc -> make_hpc_plots.py -> plots 
'''

'''
Parse command line arguments. 
Returns arguments as a list. 
'''
def parse_args(): 
    parser = argparse.ArgumentParser() 
    parser.add_argument("-f", "--pc_file", required=True, help="mn_exec_pc.PID.log file name")
    parser.add_argument("-d", "--output_dir", required=False, help="path to directory to save .hpc file if you don't want to save it to default location (path to mn_exec_pc.PID.log file)") 
    args = parser.parse_args() 
    return args 

'''
Detect duplicates in dictionary. If duplicate exist, add number onto end of key. 
'''
def detect_duplicates(ordered_pairs):
    node_dict = {}
    i = 0 # i is the number we add on to the end of duplicate keys to differentiate them 
    for key, val in ordered_pairs:
        if key in node_dict: # if duplicate key
            # Assumption: all duplicates are valid.
            # Making this assumption because no way to guarantee that a duplicate is valid/invalid
            # This is because all zeros would be equal to each other, but also valid
            new_key = key + f"{i}"  # create a new key name
            i += 1
            node_dict[new_key] = val  # add new key name and val to return dict
            print(f"WARNING: key: {key} is a duplicate! Replacing {key} with {new_key}")  # give user warning
        else: # if not a duplicate key, just add to dict 
            node_dict[key] = val
    return node_dict

'''
Get one node's JSON from concatenated log. 
'''
def get_one_node(line_num, previous_sn_line, pc_file):
    if(previous_sn_line > 0):  # this is the calculation for all nodes except first 
        tail_line = line_num - previous_sn_line - 1 
    else: # this is the calculation for the first node 
        tail_line = line_num
    cmd = f'''cat {pc_file} | head -n {line_num} | tail -n {tail_line}''' # execute command line utilities to get correct lines from file 
    print(f"Executing command: {cmd}") 
    node = subprocess.check_output(cmd, shell=True) 
    node = json.loads(node, strict=False, object_pairs_hook=detect_duplicates) # convert text from file into json, check for duplicate keys 
    return node 

'''
Separate all node jsons given concatenated log. 
'''
def separate_nodes(pc_file): 
    node_delim = " NODE sn" # NOTE: this will need to change if separator between nodes changes 
    node_jsons = [] # list of node jsons 
    f = open(pc_file) 
    lines = f.readlines() 
    previous_sn_line = 0 # assume node 0's dictionary starts on line 0 
    for i,line in enumerate(lines): # check each line in pc file 
        if node_delim in line: # if " NODE sn" is in the line, then assume next line starts new node's dictionary 
            node_json = get_one_node(i, previous_sn_line, pc_file) # get node's dictionary 
            node_jsons.append(node_json) # add this node's dictionary to list 
            previous_sn_line = i # save where this node's dictionary ended so we know where to start cutting on next node 
    # there won't be a delimeter after the last node, so have to do that one separately 
    node_json = get_one_node(len(lines), previous_sn_line, pc_file) 
    node_jsons.append(node_json) 
    return node_jsons 

'''
Given a key, determine if it is the default string or the input string from the user. 
Return input string if found, otherwise, return placeholder string. 
'''
def get_invoker_key(invoker_key, key_to_compare, invoker_node, j):
    # change pattern if time/date format changes
    pattern = r'DEFAULT_.*_COUNTERS [A-Za-z]+ [A-Za-z]+ [0-9]{2} [0-9]{2}:[0-9]{2}:[0-9]{2} [0-9]{4}' # this pattern is the default string for all the non-invoker nodes 
    match_object = re.search(pattern, key_to_compare) 
    if match_object is None: # then this is the invoker node 
         invoker_key = key_to_compare # return user's input string 
         invoker_node = j # return invoker node id 
    return invoker_key, invoker_node 

'''
Only one node should have the input string from the user. 
Figure out which node that is and what the string is. 
'''
def determine_invoker(node_jsons, i): 
    invoker_key = "invoker_key" # dummy key to keep track of whether we have found user's input 
    invoker_node = -1 # dummy node to keep track of whether we have found invoker node (invoker node has user's input string/key)
    for j,node in enumerate(node_jsons): # check each node until find invoker 
        if invoker_node != -1: # if found invoker, break out of loop, don't need to keep searching 
            break
        key_to_compare = list(node.keys())[i] # get this node's key 
        invoker_key, invoker_node = get_invoker_key(invoker_key, key_to_compare, invoker_node, j) # check to see if this node's key is default or user's input 
    if invoker_key is "invoker_key" or invoker_node is -1: # if can't find user's input, tell user and exit 
        print(f"Did not find invoker key or node for this call") 
        exit(1) 
    return invoker_key, invoker_node # return non-dummy, non-default string - assume user's input/invoker node's string 

'''
Replace a key in the master json with a new key. 
'''
def replace_key(master_json, call, invoker_key): 
    master_json[invoker_key] = master_json[call] # copy data from old key to new key 
    del master_json[call] # delete old key & data 
    return master_json 

'''
Add each node's data to the master JSON (for one performance counter operation) 
'''
def add_node_data(master_json, node_jsons, i, master_call): 
    for j,node in enumerate(node_jsons[1:]): # for each node besides node 0 
        call = list(node.keys())[i] # get the correct operation's sub-dictionary (dictionary for specific INIT/START/READ/STOP)
        if node[call] == "N/A": # if operation is INIT or START, no data to add, so skip 
            pass 
        else: # if operation is READ or STOP, add data 
            for arch in node_jsons[0][master_call]: # arch (architectures) are MSP, GC, GC_CLUSTER, SRIO_IN, SRIO_OUT
                if arch != "System_Parameters": # don't duplicate system parameters data 
                    for counter in node_jsons[0][master_call][arch]:  # add data for each counter 
                        new_key = list(node[call][arch][counter].keys())[0] # get the "NodeX" or "ClusterX" or "PortX" string 
                        master_json[master_call][arch][counter][new_key] = node[call][arch][counter][new_key] # add data 

'''
Given a json object, print to .hpc file. 
'''
def output_hpc_file(master_json, hpc_filename): 
    json_obj = json.dumps(master_json, indent=4, separators=(',', ': ')) # make a pretty json object from dictionary 
    with open(hpc_filename, "w") as outfile: 
        outfile.write(json_obj) # output final json to .hpc file 

'''
For each performance counter operation, add all data from all nodes. 
'''
def concat_nodes(node_jsons): 
    master_json = node_jsons[0].copy() # use node 0's json as scaffolding 
    for i,call in enumerate(node_jsons[0]): # for each INIT/START/READ/STOP operation
        invoker_key, invoker_node = determine_invoker(node_jsons, i) # find user's input string for this operation 
        if call != invoker_key: # if not on invoker node, update key to be user's input string 
            master_json = replace_key(master_json, call, invoker_key)        
        add_node_data(master_json, node_jsons, i, call) # add all node data to master json (except node 0 because already have that in master) 
    return master_json 

'''
Make sure all nodes have same number of performance counter operations (INIT/START/READ/STOP). 
If they don't, something went wrong. Error out. 
'''
def validate_jsons(node_jsons):
    calls = set() # use a set because adding same value to existing value in set keeps set same size 
    for node_json in node_jsons: 
        calls.add(len(node_json)) # length of json give number of INIT/START/READ/STOP operations 
    if len(calls) > 1: # len(calls) should be 1 because if all JSONS had the same number of operations, set would only have one value 
        print(f"All nodes do not have same number of performance counter operations. Something went wrong") 
        exit(1) 

'''
Return path going to save output in. Default: same place as pc file. User can specify different path. 
'''
def get_directory(output_dir, pc_file): 
    if output_dir is None: # if user did not input an output directory 
        return os.path.dirname(os.path.abspath(pc_file)) # just use the path to the pc file 
    else: # if user did input an output directory 
        if os.path.isdir(output_dir): # make sure it exists 
            return output_dir 
        else: # if user inputted directory does not exist, tell user and error out 
            print(f"{output_dir} is not a valid directory") 
            exit(1) 

'''
Return full path and filename of .hpc file. 
'''
def get_hpc_filename(pc_file, output_dir): 
    pc_dir = get_directory(output_dir, pc_file) # get directory to save hpc file in (default or user input) 
    logname = os.path.basename(pc_file) # get the pc file name (without path) 
    hpc_filename = os.path.splitext(logname)[0] + '.hpc' # strip off .log extension and add .hpc extension instead 
    hpc_full_path = '/'.join([pc_dir, hpc_filename]) # add the .hpc file name to the path 
    print(f"Saving output to: {hpc_full_path}") # tell user where going to save .hpc file 
    return hpc_full_path # return path to be used when writing to .hpc file 

def main(): 
    args = parse_args() # parse command line arguments 
    node_jsons = separate_nodes(args.pc_file) # get each node's dictionary 
    validate_jsons(node_jsons) # make sure all nodes have same number of reads 
    master_json = concat_nodes(node_jsons) # aggregate all data into one dictionary 
    hpc_filename = get_hpc_filename(args.pc_file, args.output_dir) # create .hpc filename  
    output_hpc_file(master_json, hpc_filename) # output aggregated dictionary as JSON to .hpc file 

if __name__=="__main__": 
    main() 
