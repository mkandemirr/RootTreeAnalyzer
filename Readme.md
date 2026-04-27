# RootTreeAnalyzer

RootTreeAnalyzer is a lightweight C++ toolkit for analyzing ROOT files that store event-based data with complex branch structures.

In many ROOT analysis files, branches may contain single values, vectors, or even nested vectors for each event. Working with these structures directly can become tedious, hard to read, and error-prone. 

This toolkit uses a simple object-oriented approach to provide a cleaner and more intuitive way to access event data. It provides a structured way to organize analysis code, making it easier to develop, extend, and maintain tools for event-based data analysis.

I originally created it to keep my analysis code organized while working on the ANNIE experiment. As a result, the classes in the shared/ directory and the analysis tools are specific to ANNIE. However, the overall design can also serve as a useful template for similar event-based ROOT analysis workflows.


## Design

The toolkit separates low-level ROOT I/O from higher-level analysis logic using a simple object-oriented design.

* `...Reader` classes  
  These classes connect directly to ROOT branches and handle all data access from the tree. They isolate the details of branch names, types, and memory management.

* `E...` classes  
  These are event-level containers that represent meaningful data within a single event (e.g. clusters, hits, pulses). Instead of dealing with raw vectors or nested structures, the user interacts with well-defined C++ objects.

In practice, the workflow looks like:

1. Reader classes load data from the ROOT tree  
2. Event-level objects organize this data into meaningful structures  
3. Analysis tools operate on these objects .Instead of directly accessing ROOT branches, tools work with event-level objects to perform analysis tasks.  Since the data is already organized into meaningful objects, the analysis code becomes closer to the underlying physics logic rather than low-level data handling.

This design provides:

- cleaner and more readable analysis code  
- reduced duplication across tools  
- safer handling of complex branch structures  
- easier maintenance if the ROOT structure changes  

---

## Structure

```bash
RootTreeAnalyzer/
├── shared/
├── tools/
├── iniReader/
└── CMakeLists.txt
```


### shared/

This is where you put common classes that are reused across different tools.

Typically, this includes two main types of classes:

- **Reader classes**, which handle low-level access to ROOT branches.  
- **Event classes** , which group related data into structured, physics-level objects such as hits, tracks..

By placing them here, all tools can use the same interface to access event data instead of working directly with raw ROOT branches.  
This makes the code cleaner and easier to maintain.

### tools/

This is where you put independent analysis tools. 
Each tool is implemented as a separate executable with its own `main` function.

You can build all tools or only selected ones.

### iniReader/

A lightweight single-header INI parser for configuring tools without recompilation (based on [inih](https://github.com/jtilly/inih)).

## Configuration

Each tool provides its own configuration files (e.g. `config.ini`, `mcConfig.ini`) inside its directory.

During compilation, these files are automatically copied to: `<build_dir>/configs/<tool_name>/`


## Build (Out-of-Source)

It is recommended to build the project in a separate directory:

```bash
mkdir build
cd build
cmake ../<source_dir>
cmake --build . -j
```

### Select tools to build

Build all tools:

```bash
cmake ../<source_dir>
```

Build a single tool:

```bash
cmake ../<source_dir> -DTOOLS="tools/tool1"
```

Build multiple tools:

```bash
cmake ../<source_dir> -DTOOLS="tools/tool1;tools/tool2"
```

Tool names must include the `tools/` prefix and be separated by `;`

---

## Running a tool

After building the project, each tool can be executed using its configuration file located in: `<build_dir>/configs/<tool_name>/`

For example, to run the event display tool:

```bash
./displayEvent ./configs/displayEvent/config.ini
```

## Dependencies

- ROOT
- CMake >= 3.10

