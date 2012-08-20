TEMPLATE = subdirs

# Directories
SUBDIRS += \
	Analyzer \
	BinarySearcher \
	Bookmarks \
	BreakpointManager \
	CheckVersion \
	DebuggerCore \
	DumpState \
	Environment \
	FunctionFinder \
	HardwareBreakpoints \
	OpcodeSearcher \
	References \
	ROPTool \
	SessionManager \
	StringSearcher \
	SymbolViewer \
	ProcessProperties

unix {
	!macx {
		SUBDIRS += ELFBinaryInfo
		SUBDIRS += HeapAnalyzer
	}

	linux-* {
		SUBDIRS += OpenFiles 
	}
}
