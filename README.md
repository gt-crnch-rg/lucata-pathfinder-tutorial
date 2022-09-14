# Lucata Pathfinder Tutorial

This repository maintains the latest version of this tutorial. 


## HPEC 2022 Tutorial Information

**Date/Time:** September 22nd
* 12:15 to 3:45 PM EDT
* Held virtually via the HPEC video platform
* [HPEC Preliminary Agenda](https://ieee-hpec.org/index.php/ieee-hpec-2022-prelim-agenda/)
* HPEC provides a complementary registration category, so please join us! Select the “Exploring Graph Analysis for HPC with Near-Memory Accelerators” tutorial when registering, so we know how many attendees will join. 

### HPEC Tutorial Overview

#### Abstract
The growth of applications related to machine learning, drug discovery for precision medicine, and knowledge
graphs has led to a surge in the usage of graph analysis concepts with high-performance computing platforms. At
the same time, recent APIs like [GraphBLAS](https://graphblas.org/) have introduced the concept of using “traditional”
linear algebra operators to work with large data sets represented as graphs.

This tutorial provides both an introduction to the concept of near-memory hardware accelerators to support
high-performance graph analytics as well as an in-depth hands-on session with one specific near-memory
accelerator architecture, the [Lucata Pathfinder-S](https://lucata.com/solutions/pathfinder/).
Near-memory architectures like the Pathfinder are well-suited
for graph analytics due to their focus on keeping computation very close to data. This strategy reduces 
performance penalties commonly associated with traversing graphs or
other sparse data structures.

Attendees will learn about possible use cases for graph analytics for HPC applications, and they will be able
to use a 32-node Pathfinder system to execute HPC-relevant code examples. GraphBLAS support for the
Pathfinder will be discussed, and attendees will have an opportunity to work through several benchmarks built
for the Pathfinder using the latest GraphBLAS API.

#### Goals of this Tutorial
* Attendees should understand HPC-relevant application spaces where graph analysis can be used to
process large data sets, including ML applications, knowledge graphs, and emerging fields like drug
discovery.
* Attendees will learn the basics of GraphBLAS and how it provides a bridge between traditional BLAS
libraries and graph analysis applications.
* Attendees will learn how to program and run simple Cilk-based examples based on SpMV and GEMM
that can be run on the Lucata Pathfinder-S system.
* Attendees should gain an appreciation for what a GraphBLAS application looks like, and how this can be
used with novel architectures, including the Pathfinder-S system.

#### Preliminary Agenda (May be tweaked slightly before the event)
| **Time (All Times EDT) | **Topic**                                      |
| ---------------------- | ---------------------------------------------- |
| 12:15                  | Overview and Introduction                      |
| 12:35                  | Introduction to the Lucata Pathfinder-S System |
| 1:00                   | Programming Basics, SAXPY                      |
| 1:45                   | **BREAK**                                      |
| 2:15                   | Workflow Discussion                            |
| 2:45                   | GraphBLAS                                      |
| 3:25                   | Profiling/Timing; Hands-on Investigation       |
| 3:45                   | Wrap-up                                        |


## Software Information
This tutorial is geared towards using the 22.02 version of the Lucata programming framework. Please see the [programming manual (requires RG and GT account)](https://github.gatech.edu/crnch-rg/rg-lucata-pathfinder/blob/main/docs/pathfinder/Lucata-Pathfinder-Programming-Guide-v2.0.0-2202-tools.pdf) for more information. 


## Acknowledgments
This tutorial information is funded in part by NSF CNS #2016701, [“Rogues Gallery: A Community Research Infrastructure for Post-Moore Computing”](https://www.nsf.gov/awardsearch/showAward?AWD_ID=2016701). We are greatly appreciative of the assistance from Lucata for slides that we have included as well as general support and assistance in supporting the Pathfinder platform.

### Previous Versions of This Tutorial

The initial version of the tutorial material was developed by Janice McMahon with help from Jeffrey Young for the notebooks. 

* [PEARC 2021](https://github.com/gt-crnch-rg/pearc-tutorial-2021)
* PEARC 2019
* ASPLOS 2019
