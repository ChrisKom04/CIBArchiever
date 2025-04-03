# CIBArchiver

CIBArchiver is an archiving tool designed to store files and directories while maintaining their hierarchical structure. It also supports data compression using the gzip(1) program. This project was assigned in the Operating Systems course and involves extensive use of low-level system calls.

All code in this commit is written solely by me.

## CIB File Description

The CIB file is a custom binary file format used by the CIBArchiver project to store and manage files and directories in a structured and efficient manner. It ensures data integrity, supports compression, and maintains the hierarchical organization of the archived content. 

### Structure of a CIB File

A CIB file is organized into the following sections:

1. **Header Section**: Contains info about the file, such as size, base directory and configuration details.
2. **Data Blocks**: Stores the actual data in a structured format.
3. **Metadata Section**: Maintains the file and directory hierarchy, enabling efficient organization.

### Visual Representation

Below is a visual representation of the CIB file structure:

```
+-------------------+
| Header Section    |
+-------------------+
| Data Section      |
|-------------------|
| Free Data Blocks  |
|      List         |
|-------------------|
| Data Block 1      |
| Data Block 2      |
| ...               |
+-------------------+
| Metadata Section  |
|-------------------|
|   Free Metadata   |
|    Blocks List    |
|-------------------|
| Metadata Block 1  |
| Metadata Block 2  |
| ...               |
+-------------------+
```

`Free Data Blocks List` and `Free Metadata Blocks List` are data structures used to efficiently track free
space in each section.

## Supported Operations

The `cib` command-line tool provides the following operations for managing `.cib` archive files:

1. **Create a New Archive (`-c`)**
   - Creates a new archive file and adds the specified files and/or directories to it.
   - **Usage:** `cib -c <archive-file> <list-of-files/dirs>`
   - Example: `cib -c archive.cib file1 file2 dir1`

2. **Append to an Existing Archive (`-a`)**
   - Adds files or directories to an existing archive.
   - **Usage:** `cib -a <archive-file> <list-of-files/dirs>`
   - Example: `cib -a archive.cib file3 dir2`

3. **Extract Files or Directories (`-x`)**
   - Extracts the contents of the archive to the current directory. If no specific files or directories are provided, it extracts everything.
   - **Usage:** `cib -x <archive-file> [list-of-files/dirs]`
   - Example: `cib -x archive.cib` or `cib -x archive.cib file1 dir1`

4. **Compress the Archive (`-j`)**
   - Compresses the archive using gzip. This flag is used in combination with `-c` or `-a`.
   - **Usage:** `cib -c -j <archive-file> <list-of-files/dirs>` or `cib -a -j <archive-file> <list-of-files/dirs>`
   - Example: `cib -c -j archive.cib file1` or `cib -a -j archive.cib file2`

5. **Delete Files or Directories (`-d`)**
   - Removes specified files or directories from the archive.
   - **Usage:** `cib -d <archive-file> <list-of-files/dirs>`
   - Example: `cib -d archive.cib file1 dir1`

6. **Print Metadata (`-m`)**
   - Displays metadata of the stored items in the archive.
   - **Usage:** `cib -m <archive-file>`
   - Example: `cib -m archive.cib`

7. **Check Existence (`-q`)**
   - Checks if specific files or directories exist in the archive.
   - **Usage:** `cib -q <archive-file> <list-of-files/dirs>`
   - Example: `cib -q archive.cib file1 dir1`

8. **Print Archive Structure (`-p`)**
   - Outputs a human-readable structure of the archive.
   - **Usage:** `cib -p <archive-file>`
   - Example: `cib -p archive.cib`

Note: The `.cib` archive can only include files or directories located under the current working directory. For example, if the current working directory is `/home/userx`, the `.cib` archive can only contain paths like `/home/userx/test_dir/test_file1`.

## Getting Started

### Prerequisites
- A C compiler (e.g., GCC)
- Make utility
- `gzip` installed

### Building the Project
Run the following command to build the project:
```sh
make
