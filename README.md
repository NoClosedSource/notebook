<p align="center">
  <img src="https://github.com/NoClosedSource/notebook/blob/d1e4eb4b64c711588327d1bc8215fb34d035d985/resources/NotebookIcon.png" alt="drawing" width="100" align="center"/>
  <h1 align="center">Notebook</h1>
</p>

<p align="center">
  A simple, lightweight text editor with a bunch of basic features.
</p>

Notebook is a text editor created in C++ intended for users who don't need anything fancy or big to change some words in a text file.

# Features
- Undo/Redo with a Max History Limit
- Customizable Max History Limit (default 100)
- Searching with Ctrl + F
- Replacing with Ctrl + G
- Zooming in & out with Ctrl + Plus/Minus

# Installation
Here's how to go about installing Notebook

**From Source**
1. Copy main.cpp
2. Compile with GCC
```
gcc main.cpp -o notebook `pkg-config --cflags --libs gtk+-3.0`
```
