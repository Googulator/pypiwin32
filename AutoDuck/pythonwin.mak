# MAKEFILE
# Builds documentation for Pythonwin using the AUTODUCK tool
#

!include "common_top.mak"

TARGET  = PythonWin
GENDIR  = ..\build\Temp\Help
TITLE   = $(TARGET) Help
DOCHDR  = $(TARGET) Reference

SOURCE_DIR = ../pythonwin
SOURCE  = $(SOURCE_DIR)\contents.d $(SOURCE_DIR)\*.cpp $(SOURCE_DIR)\*.h 

# Help and Doc targets

hwlp : $(GENDIR) ..\$(TARGET).hlp

doc : $(TARGET).doc

clean: cleanad

!include "common.mak"
