#include "widget.hpp"

// all implementation lives in subclasses (Label, Button, Textbox, List)
// this file exists so widget.cpp appears in the build and the vtable
// is emitted here rather than in every translation unit
