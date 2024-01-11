#!/usr/bin/env python3

'''
Purpose of this script: Create csv file and graphs showing the value of polled registers over profiling time
mn_exec_pr.PID.log -> emu_analyze_pr.py -> csv & plots
'''

import os
import argparse
import subprocess
import json
import re
import random
import pandas as pd
import matplotlib.pyplot as plt
import copy
import datetime
import sys
import numpy as np
from enum import IntEnum

'''
Global Variables
'''

class Fig_enums(IntEnum): # use this enum to define which figure we are modifying
    LINE = 1
    STACKED = 2
    SUBPLOTS = 3
    LINE_ALL_NODES = 4
    STACKED_ALL_NODES = 5
    SUBPLOTS_ALL_NODES = 6

DEFAULT_ROTATION = 45 # default rotation of the START/STOP/READ strings
GC_CONTEXTS_COLUMN_NAME = "OCCUPIED_CONTEXTS"
CONTEXTS_PER_GC = 64
MAX_THREADS = 1024

'''
Parse command line arguments.
Returns arguments as a list.
'''
def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("-f", "--pr_file", required=True, help="mn_exec_pr.PID.log file name")
    parser.add_argument("-d", "--output_dir", required=False, help="path to directory to save output files to if you don't want to save it to default location (path to mn_exec_pr.PID.log file)")
    parser.add_argument("-p", "--prefix", required=False, help="Add prefix to graph directory name and graph names")
    parser.add_argument("-n", "--no_graphs", required=False, help="only make csvs, no graphs", action="store_true")
    parser.add_argument("-c", "--choose_intervals", metavar="NUM_INTERVALS", required=False, help="choose which intervals to graph")
    parser.add_argument("-v", "--no_csv", required=False, help="do not create csv", action="store_true")
    parser.add_argument("-l", "--no_max_line", required=False, help="do not add max threads line to threads graphs", action="store_true") # you might want to use this if your # threads is much lower than max (1536)
    parser.add_argument("-s", "--stacked_gc_plot", required=False, help="graph GC CONTEXTS as stacked area plot", action="store_true")
    parser.add_argument("-b", "--gc_subplots", required=False, help="graph GC CONTEXTS as one subplot per GC", action="store_true")
    parser.add_argument("-i", "--input_csv", required=False, help="path to existing csv, don't have to re-create. CAN SAVE A LOT OF TIME") # with this, don't need pr file technically but haven't made that possible yet
    # NOTE: if want option to graph available contexts instead of occupied, need to change the stacked plot to subtract instead of add counts
    parser.set_defaults(feature=False)
    args = parser.parse_args()
    if args.prefix and args.choose_intervals:
        print(f"When you choose intervals, you will choose a prefix for each interval. There is no need to have both the --prefix and --choose_intervals flags.")
        parser.print_help()
        exit(1)
    if args.choose_intervals and args.no_graphs:
        print(f"Choosing intervals applies to graphs. You cannot have both the --choose_intervals flag and the --no_graphs flag")
        parser.print_help()
        exit(1)
    return args

'''
Return path going to save output in. Default: same place as pr file. User can specify different path.
'''
def get_directory(output_dir, pr_file):
    if output_dir is None: # if user did not input an output directory
        return os.path.dirname(os.path.abspath(pr_file)) # just use the path to the pr file
    else: # if user did input an output directory
        if os.path.isdir(output_dir): # make sure it exists
            return output_dir
        else: # if user inputted directory that does not exist, tell user and error out
            print(f"{output_dir} is not a valid directory")
            exit(1)

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
            new_key = key + f"_{i}"  # create a new key name
            i += 1
            node_dict[new_key] = val  # add new key name and val to return dict
            # print(f"WARNING: key: {key} is a duplicate! Replacing {key} with {new_key}")  # give user warning
        else: # if not a duplicate key, just add to dict
            node_dict[key] = val
    return node_dict

'''
Get one node's JSON from concatenated log.
'''
def get_one_node(line_num, previous_sn_line, pc_file, node):
    if(previous_sn_line > 0):  # this is the calculation for all nodes except first
        tail_line = line_num - previous_sn_line - 1
    else: # this is the calculation for the first node
        tail_line = line_num
    cmd = f'''cat {pc_file} | head -n {line_num} | tail -n {tail_line}''' # execute command line utilities to get correct lines from file
    print(f"{node} Executing command: {cmd}")
    node = subprocess.check_output(cmd, shell=True)
    node = json.loads(node, strict=False, object_pairs_hook=detect_duplicates) # convert text from file into json, check for duplicate keys
    return node

'''
Separate all node jsons given concatenated log.
'''
def separate_nodes(pr_file):
    node_delim = " NODE sn" # NOTE: this will need to change if separator between nodes changes
    node_jsons = [] # list of node jsons
    f = open(pr_file)
    lines = f.readlines()
    previous_sn_line = 0 # assume node 0's dictionary starts on line 0
    node = 0
    for i,line in enumerate(lines): # check each line in pr file
        if node_delim in line: # if " NODE sn" is in the line, then assume next line starts new node's dictionary
            node_json = get_one_node(i, previous_sn_line, pr_file, node) # get node's dictionary
            node += 1
            node_jsons.append(node_json) # add this node's dictionary to list
            previous_sn_line = i # save where this node's dictionary ended so we know where to start cutting on next node
    # there won't be a delimeter after the last node, so have to do that one separately
    node_json = get_one_node(len(lines), previous_sn_line, pr_file, node)
    node_jsons.append(node_json)
    return node_jsons

'''
Look to see if reg_name is a key in the dictionary at node_json[key]
If so, turn the value (which looks like a string) into a python list object
'''
def adjust_reg_vals_list(node_json, key, reg_name, invert, subtract_val):
    if reg_name in node_json[key]: # if there are values to manipulate
        node_json[key][reg_name] = list(map(int, node_json[key][reg_name].split(','))) # split string by commas, turn all values into integers, then turn entire thing into python list object
        if invert: # TODO: make this a command line option
            node_json[key][reg_name] = list(map(lambda x: subtract_val - x, node_json[key][reg_name])) # subtract each value in list from specified subtract val
        num_vals = len(node_json[key][reg_name]) # get how many values are in list
        return num_vals
    return -1



'''
Add data from node_json to data frame
'''
def add_to_dataframe(i, node_json, df, input_strings):
    local_input_strings = copy.deepcopy(input_strings) # create copy of input strings for this node, have to deep copy so don't pop off of master list passed in for each node
    num_gcs = -1 # -1 will signify that there is not GC data in this run
    dfs_to_concat = list() # init list of rows/dataframes to concat together
    for key in node_json:
        if isinstance(node_json[key], dict): # if the value is a dictionary, then this is a reading of a register
            num_gcs = adjust_reg_vals_list(node_json, key, 'AVAILABLE_CONTEXTS', True, CONTEXTS_PER_GC) # turn list into python list object and subtract all values from 64 NOTE: if don't call this function, need to set GC_CONTEXTS_COLUMN_NAME
            dfs_to_concat.append(pd.DataFrame([node_json[key]])) # add this row to list of rows to concat
        else: # if this is a READ/START/STOP string
            # assumption: user's input strings are on node 0
            # assumption: start/reads/stops are in order
            # assumption: every node has same # of stops/starts/reads
            if i == 0: # node 0 has the user's input strings instead of DEFAULT ones
                local_input_strings.append(key) # add the user's strings to a list
                new_key = key + "_NODE" + str(i) # differentiate key for node 0
            else:
                new_key = local_input_strings.pop(0) + "_NODE" + str(i) # differentiate key for this node
            hpc_str_input = {}
            hpc_str_input["hpc_str_input"] = new_key # use user's input string + NODE instead of DEFAULT
            hpc_str_input["REALTIME"] = float(node_json[key]) # add timestamp to this row
            hpc_str_input["NODE"] = i # add node to this row
            if len(dfs_to_concat) > 0: # if this isn't the first read/start/stop
                dfs_to_concat.insert(0, df) # add the existing df to beginning
                df = pd.concat(dfs_to_concat, ignore_index=True, sort=False) # add rows for prev read/start/stop to df
            dfs_to_concat = list() # start a new list of dfs to concat
            dfs_to_concat.append(pd.DataFrame([hpc_str_input])) # add read/start/stop row to beginning of new df
    # make sure to add the last read/start/stop
    dfs_to_concat.insert(0, df) # add existing df to beginning
    df = pd.concat(dfs_to_concat, ignore_index=True, sort=False) # add rows for prev read/start/stop to df
    if i == 0: # if on node 0, populate master list of strings with local strings
        input_strings = local_input_strings
        if len(input_strings) < 1:
            print_exception_exit(f"ERROR: Found 0 READ/START/STOPs")
    return df, input_strings, num_gcs

'''
Add horizontal line to current fig at desired y value
'''
def add_hz_line(hz_bool, hz_y, hz_color, hz_label):
    if hz_bool:
        plt.axhline(y=hz_y, color=hz_color, label=hz_label)

'''
Save figure and close figure
'''
def save_close_fig(fig_title):
    title = fig_title + ".png"
    plt.savefig(title)
    plt.close()

'''
Subplots need special adjustments
'''
def finish_subplots_fig(all_nodes, obj, graph_title, fig_title, legend_strs):
    if all_nodes:
        obj.legend(legend_strs, loc='upper left') # put legend on fig
    obj.suptitle(graph_title) # add fig title
    obj.tight_layout()
    plt.subplots_adjust(top=0.95) # make room for fig title
    save_close_fig(fig_title)

'''
Add title, labels, legend to figure and save as desired name.
'''
def finish_fig(fig_exists, fig_num, graph_title, ylabel, fig_title, hz_bool, hz_y, hz_color, hz_label, node, legend_strs):
    if fig_exists:
        plt.figure(fig_num) # make sure we are working on desired figure
        add_hz_line(hz_bool, hz_y, hz_color, hz_label) # add horizontal line to figure if desired
        if fig_num < Fig_enums.LINE_ALL_NODES: # if fig is for 1 node
            fig_title = fig_title + "_N" + str(node) # make fig title uniq with NODE ID
            if fig_num == Fig_enums.SUBPLOTS:
                finish_subplots_fig(False, plt, graph_title, fig_title, None)
                return
            plt.legend(loc='upper left') # must do this after subplots because subplots can't have
        else: # if fig is for all nodes
            if fig_num == Fig_enums.SUBPLOTS_ALL_NODES:
                finish_subplots_fig(True, plt.figure(fig_num), graph_title, fig_title, legend_strs)
                return
            plt.legend(legend_strs, loc='upper left') # must do this after subplots because subplots plt vs. obj different
        plt.title(graph_title)
        plt.xlabel("time")
        plt.ylabel(ylabel)
        plt.xticks(rotation=45)
        plt.tight_layout()
        save_close_fig(fig_title)

'''
Setup figure and subplot
'''
def setup_fig(fig_num):
    plt.figure(fig_num, figsize=(24,18)) # TODO: do we want to change figure size?

'''
Get max value across all columns specified by reg_names
'''
def get_max_all_regs(this_df, reg_names):
    max_all_regs = this_df[reg_names].max().max() # first max returns max in each col, second max returns max of maxes
    return max_all_regs

'''
Switch to correct fig & plot data.
'''
def plot_on_fig(fig_num, times, counts, this_drawstyle, this_label, this_color, subplots, subplot_nrows, subplot_ncols, subplot_index, ylim_max):
    setup_fig(fig_num)
    if subplots:
        plt.subplot(subplot_nrows, subplot_ncols, subplot_index)
        plt.ylim(0, ylim_max) # make it easy to set all subplots to same max y
    plt.plot(times, counts, drawstyle=this_drawstyle, label=this_label, color=this_color)

'''
This function is the workhorse of plotting
Plot register on each node over time
'''
def plot_reg_over_time(df, reg_names, node, reg_graph_colors, node_graph_colors, one_color_per_node, want_line_plot, want_stacked_plot, want_subplots, num_nodes, max_all_regs, hz_bool, hz_y):
    node_df = df[df['NODE'] == node] # only get rows with current node
    max_count = get_max_all_regs(node_df, reg_names)
    prev_counts = 0
    for i,reg_name in enumerate(reg_names):
        color_index = node if one_color_per_node else i
        reg_df = node_df[node_df[reg_name].notnull()] # exclude rows where count is null
        times = reg_df['RELATIVE_TIME'] # get times from those rows
        counts = reg_df[reg_name] # get values of register
        this_label = f"Node{node}_{reg_name}"
        this_drawstyle = 'steps-post'
        '''
        SUBPLOTS
        '''
        if want_subplots:
            ylimit = get_max_all_regs(df, [reg_name]) + 5
            add_string = True if i == (len(reg_names) - 1) else False
            nrows = len(reg_names)
            ncols = 1
            index = i + 1
            string_loc = -1 * ylimit
            string_rotation = 0
            # NOTE: strings are horizontal because don't know how long string will be, and may overlap with data if vertical/angled
            # single node
            plot_on_fig(Fig_enums.SUBPLOTS, times, counts, this_drawstyle, this_label, reg_graph_colors[color_index], True, nrows, ncols, index, ylimit)
            plt.legend(loc='upper left')
            add_strings_to_fig(node_df, Fig_enums.SUBPLOTS, node_graph_colors[node], string_loc, string_rotation, add_string)
            # all nodes
            plot_on_fig(Fig_enums.SUBPLOTS_ALL_NODES, times, counts, this_drawstyle, this_label, node_graph_colors[node], True, nrows, ncols, index, ylimit)
            add_strings_to_fig(node_df, Fig_enums.SUBPLOTS_ALL_NODES, node_graph_colors[node], string_loc, string_rotation, add_string)
        '''
        LINE
        '''
        if want_line_plot:
            plot_on_fig(Fig_enums.LINE, times, counts, this_drawstyle, this_label, reg_graph_colors[color_index], False, None, None, None, None) # single node
            plot_on_fig(Fig_enums.LINE_ALL_NODES, times, counts, this_drawstyle, this_label, node_graph_colors[node], False, None, None, None, None) # all nodes
            if i == 0:
                add_strings_to_fig(node_df, Fig_enums.LINE, node_graph_colors[node], max_count/2, DEFAULT_ROTATION, True)
                ymax = max(max_all_regs, hz_y) if hz_bool else max_all_regs
                string_spacing = (ymax/num_nodes)*node
                add_strings_to_fig(node_df, Fig_enums.LINE_ALL_NODES, node_graph_colors[node], string_spacing, DEFAULT_ROTATION, True)

        '''
        STACKED
        '''
        if want_stacked_plot: # stacked must go last because change counts
            counts = counts + prev_counts # this only works for the first one if prev_counts is initialized to 0
            all_nodes_label = this_label if i == 0 else '_nolegend_'
            # single node
            plot_on_fig(Fig_enums.STACKED, times, counts, this_drawstyle, this_label, reg_graph_colors[color_index], False, None, None, None, None)
            plt.fill_between(times, prev_counts, counts, color=reg_graph_colors[color_index], step='post')
            # all nodes
            plot_on_fig(Fig_enums.STACKED_ALL_NODES, times, counts, this_drawstyle, all_nodes_label, node_graph_colors[node], False, None, None, None, None)
            plt.fill_between(times, prev_counts, counts, color=node_graph_colors[node], step='post')
            if i == 0: # add strings only once to the fig with all nodes together
                max_at_once = df[reg_names].sum(axis=1).max()
                string_spacing = (max_at_once/num_nodes)*node
                add_strings_to_fig(node_df, Fig_enums.STACKED_ALL_NODES, node_graph_colors[node], string_spacing, DEFAULT_ROTATION, True)
                add_strings_to_fig(node_df, Fig_enums.STACKED, node_graph_colors[node], max_count/2, DEFAULT_ROTATION, True)
            prev_counts = counts
    return node_df, max_count

'''
Get just the READ/START/STOP strings and their corresponding timestamps
'''
def get_strings_xy(node_df):
    strings_df = node_df[node_df['hpc_str_input'].notnull()] # only care about rows with strings
    string_times = list(strings_df['RELATIVE_TIME']) # get time stamps
    string_strings = list(strings_df['hpc_str_input']) # get corresponding strings
    return string_times, string_strings

'''
Graph a vertical line for each READ/START/STOP
'''
def graph_hpc_calls(string_times, string_strings, this_color, string_spacing, rotate, add_string):
    for i,time in enumerate(string_times,start=0):
        if add_string:
            plt.text(time, string_spacing, string_strings[i], rotation=rotate, color=this_color)
        plt.axvline(x=time, linestyle='--', color=this_color, label='_nolegend_')

'''
Switch to correct fig & graph strings & matching vertical lines
# NOTE: could combine this function with graph_hpc_calls()
'''
def add_strings_to_fig(node_df, this_fig, this_color, string_spacing, rotate, add_string):
    plt.figure(this_fig)
    string_times, string_strings = get_strings_xy(node_df)
    graph_hpc_calls(string_times, string_strings, this_color, string_spacing, rotate, add_string)

'''
Graph data from each node in its own figure
'''
def graph_nodes(df, title, num_nodes, reg_graph_colors, node_graph_colors, reg_names, hz_bool, hz_y, hz_color, hz_label, graph_title, ylabel, want_line_plot, want_stacked_plot, want_subplots, subplots_title, stacked_title):
    max_all_regs = get_max_all_regs(df, reg_names)
    for node in range(0, num_nodes):
        one_color_per_node = True if len(reg_names) == 1 else False # need more than one color per node if have more than one register per node
        node_df,max_count = plot_reg_over_time(df, reg_names, node, reg_graph_colors, node_graph_colors, one_color_per_node, want_line_plot, want_stacked_plot, want_subplots, num_nodes, max_all_regs, hz_bool, hz_y)
        # finish single node figs
        finish_fig(want_line_plot, Fig_enums.LINE, graph_title, ylabel, title, hz_bool, hz_y, hz_color, hz_label, node, None)
        finish_fig(want_subplots, Fig_enums.SUBPLOTS, graph_title, ylabel, subplots_title, hz_bool, hz_y, hz_color, hz_label, node, None)
        finish_fig(want_stacked_plot, Fig_enums.STACKED, graph_title, ylabel, stacked_title, hz_bool, hz_y, hz_color, hz_label, node, None)
    # finish figs with all nodes together
    node_names = ["Node"+str(i) for i in range(0,num_nodes)]
    finish_fig(want_line_plot, Fig_enums.LINE_ALL_NODES, graph_title, ylabel, title + "_ALL", hz_bool, hz_y, hz_color, hz_label, node, node_names)
    finish_fig(want_subplots, Fig_enums.SUBPLOTS_ALL_NODES, graph_title, ylabel, subplots_title + "_ALL", hz_bool, hz_y, hz_color, hz_label, node, node_names)
    finish_fig(want_stacked_plot, Fig_enums.STACKED_ALL_NODES, graph_title, ylabel, stacked_title + "_ALL", hz_bool, hz_y, hz_color, hz_label, node, node_names)

'''
Add column with time relative to first/lowest time
'''
def add_relative_time_col(df):
    t0 = df['REALTIME'].min() # find t0 (minimum REALTIME)
    df['RELATIVE_TIME'] = df['REALTIME'].apply(lambda x: x - t0) # add column with relative time to t0
    return df

'''
Called inside an "Exception as e" block.
Prints error and then exits entire program.
'''
def print_exception_exit(e):
    print(f"{e}")  # print exception message
    sys.exit(1)  # exit entire program

'''
Tries to create a directory given the directory name.
If directory cannot be created, prints error and exits.
'''
def try_mkdir(dir_components):
    dir_name = "".join(dir_components)
    try:
        os.mkdir(dir_name)
    except Exception as e:
        print(f"Cannot make dir: {dir_name}")
        print_exception_exit(e)
    return dir_name

'''
Create directory to save csv/graphs in
'''
def get_save_dir(prefix, input_dir, program_name):
    now = datetime.datetime.now().strftime("%d-%m-%Y_%H:%M:%S")  # get current date and time & format
    if prefix:
        program_name = prefix + "_" + program_name
    save_dir = try_mkdir([input_dir, '/' , program_name ,'_' , now])  # concatenate all pieces into one string
    return save_dir

'''
Return float (between 0.1 and 0.9) representing one color channel.
'''
def get_color_channel():
    return random.randrange(1,9)/10

'''
Return a tuple with r,g,b values (one color).
'''
def get_color():
    return (get_color_channel(), get_color_channel(), get_color_channel())

'''
Return dictionary with read messages as keys and unique colors as values.
'''
def pick_graph_colors(num_nodes):
    graph_colors = {}
    for node in range(0, num_nodes):
        color = get_color()
        while color in graph_colors.values(): # make sure that colors not repeated
            color = get_color()
        graph_colors[node] = color
    return graph_colors

'''
Get options for start/stops
Ask user when they want to start/stop
Prune dataframe to specified interval
'''
def choose_time_intervals(df, num_nodes):
    interval_options = df[df['NODE'] == 0] # only get rows with current node
    interval_options = interval_options[interval_options["hpc_str_input"].notnull()] # this should be if hpc_str_input is not NULL
    string_strings = list(interval_options['hpc_str_input']) # get corresponding strings
    string_strings = ["_".join(s.split('_')[0:-1]) for s in string_strings]
    start_call_i,start_call = pick_call(string_strings, "start")
    print(f"You have chosen to start graphing at {start_call_i} {start_call}")
    stop_call_i,stop_call = pick_call(string_strings, "stop")
    print(f"You have chosen to stop graphing at {stop_call_i} {stop_call}")
    if stop_call_i < start_call_i:
        print(f"STOP must be after START")
        exit(1)
    df = prune_df_to_interval(df, start_call, stop_call, num_nodes)
    df = add_relative_time_col(df) # TODO: need to do df = ?
    return df,start_call

'''
Get file path & name to subplots or stacked plot graphs
'''
def get_specific_graph_filename(want_plot, dir_name, parent_dir, pr_file_base):
    specific_graph_filename = ""
    if want_plot:
        this_dir = try_mkdir([parent_dir, f"/{dir_name}"])
        specific_graph_filename = parent_dir + f"/{dir_name}/" + pr_file_base
    return specific_graph_filename

'''
Create graphs of registers one per node & one with all nodes
'''
def graph_regs(df, num_nodes, graph_colors, save_dir, pr_file_base, num_gcs, want_line_plot, want_stacked_plot, want_subplots, reg_folder_name, graph_title, ylabel, reg_names, gc_graph_colors, graph_max_line, hz_y, hz_color, hz_label):
    print(f'''Creating graphs for {reg_folder_name}''')
    gc_dir = try_mkdir([save_dir, f"/{reg_folder_name}"]) # create directory for these register(s)
    graph_filename = gc_dir + "/" + pr_file_base # create the first part of all the LINE graph's filenames
    graph_filename_stacked = get_specific_graph_filename(want_stacked_plot, "STACKED", gc_dir, pr_file_base) # create the first part of all the STACKED graph's filenames
    graph_filename_subplots = get_specific_graph_filename(want_subplots, "SUBPLOTS", gc_dir, pr_file_base) # create the first part of all the SUBPLOTS graph's filenames
    graph_nodes(df, graph_filename, num_nodes, gc_graph_colors, graph_colors, reg_names, graph_max_line, hz_y, hz_color, hz_label, graph_title, ylabel, want_line_plot, want_stacked_plot, want_subplots, graph_filename_subplots, graph_filename_stacked) # graph data

'''
Ask user what calls they want to start/stop at
'''
def pick_call(calls, op):
    for i, call in enumerate(calls):
        print(f"{i} {call}")
    s_call = input(f"What call do you want to {op} at? Please enter one integer: ")
    s_call = s_call.strip().split(' ')
    if len(s_call) > 1:
       print_exception_exit("You entered more than one number")
    s_call = int(s_call[0])
    if s_call > (len(calls)-1):
        print_exception_exit("You entered a call out of bounds")
    return s_call,calls[s_call]

'''
Given the string, get when that string was printed to pr file
'''
def get_time(s, node, df):
    s_str = s + "_NODE" + str(node)
    row = df[df['hpc_str_input'] == s_str]
    time = list(row["RELATIVE_TIME"])
    return time[0]

'''
Chop off all data before/after chosen interval.
Return dataframe with data with start & stop time stamps.
'''
def prune_df_to_interval(df, start, stop, num_nodes):
    for node in range(0,num_nodes):
        start_time = get_time(start, node, df)
        stop_time = get_time(stop, node, df)
        df = df[((df['RELATIVE_TIME'] >= start_time) & (df['NODE'] == node)) | (df['NODE'] != node)]
        df = df[((df['RELATIVE_TIME'] <= stop_time) & (df['NODE'] == node)) | (df['NODE'] != node)]
    return df

'''
Create separate column for each GC & add correct data to these columns.
'''
def split_gcs(df, num_gcs):
    dummy_gcs = [-1] * num_gcs
    df['AVAILABLE_CONTEXTS'] = df['AVAILABLE_CONTEXTS'].apply(lambda x: dummy_gcs if type(x) != list else x)
    col_names = ["GC"+str(i) for i in range(0,num_gcs)]
    gc_df = pd.DataFrame(df['AVAILABLE_CONTEXTS'].to_list(), index=df.index, columns=col_names)
    #gc_df = pd.DataFrame(df[df['AVAILABLE_CONTEXTS'].notnull()]['AVAILABLE_CONTEXTS'].to_list(), columns=col_names)
    # df[df['AVAILABLE_CONTEXTS'].notnull()] - get rows of dataframe where AVAILABLE_CONTEXTS is not null
    # ['AVAILABLE_CONTEXTS'] - get the AVAILABLE_CONTEXTS column
    # to_list() - convert to list of lists
    # pd.DataFrame - convert list to dataframe
    # removing the null rows changes the indicies :/
    df = pd.concat([df, gc_df], axis=1) # add new datframe to right of old datframe
    df = df.replace(-1, np.nan)
    df['AVAILABLE_CONTEXTS'] = df['AVAILABLE_CONTEXTS'].apply(lambda x: np.nan if x==dummy_gcs else x) # there is probably a much more efficient way to do this
    return df

'''
Given node jsons, put data into dataframe
'''
def create_dataframe(node_jsons):
    df = pd.DataFrame() # create empty data frame
    input_strings = [] # create list to hold user's input strings

    # add data from mn_exec_pr file to dataframe
    # assumption: node jsons will be in order
    for i,node_json in enumerate(node_jsons, start=0):
        df,input_strings,num_gcs = add_to_dataframe(i, node_json, df, input_strings)
    df = add_relative_time_col(df) # add column that calculates relative time
    if num_gcs > -1:
        df = split_gcs(df, num_gcs) # split list of GCs into their own cols

        df.rename(columns={'AVAILABLE_CONTEXTS': GC_CONTEXTS_COLUMN_NAME}, inplace=True) # rename column
    return df,num_gcs

'''
Save dataframe data as csv
'''
def save_csv(no_csv, df, save_dir, pr_file_base):
    if not no_csv:
        csv_name = save_dir + "/" + pr_file_base + ".csv" # create name of csv file
        print(f"Find csv in: {csv_name}")
        df.to_csv(csv_name, encoding='utf-8') # convert dataframe to csv

'''
Create graphs for threads and occupied gc contexts
'''
def create_graphs(df, num_nodes, save_dir, pr_file_base, num_gcs, no_max_line, stacked_gc_plot, gc_subplots):
    graph_colors = pick_graph_colors(num_nodes) # pick color for each node #
    if "NUM_THREADS" in df.columns.values:
        if num_gcs != 0:
            max_contexts = num_gcs * CONTEXTS_PER_GC
        else: # there is no way to know the number of contexts if GCs are not in the data
            max_contexts = MAX_THREADS
        max_line_label = str(max_contexts) + " threads"
        graph_regs(df, num_nodes, graph_colors, save_dir, pr_file_base, num_gcs, True, False, False, "THREADS", "THREADS OVER TIME", "NUM_THREADS", ["NUM_THREADS"], graph_colors, not no_max_line, max_contexts, 'black', max_line_label)
    if (GC_CONTEXTS_COLUMN_NAME in df.columns.values) and (stacked_gc_plot or gc_subplots):
        graph_regs(df, num_nodes, graph_colors, save_dir, pr_file_base, num_gcs, False, stacked_gc_plot, gc_subplots, "GC_CONTEXTS", "OCCUPIED_GC_CONTEXTS OVER TIME", "OCCUPIED_GC_CONTEXTS", ["GC"+str(i) for i in range(0,num_gcs)], pick_graph_colors(num_gcs), False, None, None, None)
    print(f"Find all graphs in: {save_dir}")  # tell user where to find graphs

def main():
    args = parse_args() # parse command line arguments
    # TODO: don't need pr file if inputting csv
    pr_file_base = args.pr_file.split('/')[-1] # get mn_exec_pr part of path
    input_dir = get_directory(args.output_dir, args.pr_file) # determine dir to save in
    save_dir = get_save_dir(args.prefix, input_dir, pr_file_base)
    if args.input_csv:
        df = pd.read_csv(args.input_csv)
        num_nodes = df['NODE'].nunique()
        num_gcs = len(list(filter(lambda x: x.startswith("GC") ,list(df.columns.values))))
        # NOTE: if don't reverse gc count (- 64), will need to change GC_CONTEXTS_COLUMN_NAME here
    else:
        node_jsons = separate_nodes(args.pr_file) # get each node's dictionary
        num_nodes = len(node_jsons)
        df,num_gcs = create_dataframe(node_jsons)
        save_csv(args.no_csv, df, save_dir, pr_file_base)
    if args.no_graphs: # exit if user doesn't want graphs
        exit(0)

    # create graphs
    pr_file_base = args.prefix + "_" + pr_file_base if args.prefix else pr_file_base # assumption: after this point, want the prefix already added to pr_file_base

    if args.choose_intervals:
        for choice in range(int(args.choose_intervals)): # choose time intervals to graph
            int_df,start_str = choose_time_intervals(df, num_nodes)
            int_prefix = input(f"What would you like the prefix for this time interval to be?: ")
            # TODO: validate int_prefix?
            print(f"you have chosen to prefix the directory for this time interval with: {int_prefix}")
            int_dir = try_mkdir([save_dir, "/", int_prefix])
            int_pr_file_base = int_prefix + "_" + pr_file_base
            create_graphs(int_df, num_nodes, int_dir, int_pr_file_base, num_gcs, args.no_max_line, args.stacked_gc_plot, args.gc_subplots)
    else:
        create_graphs(df, num_nodes, save_dir, pr_file_base, num_gcs, args.no_max_line, args.stacked_gc_plot, args.gc_subplots)

if __name__=="__main__":
    main()
