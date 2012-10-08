TEMPLATE = subdirs

# Directories
SUBDIRS += \
	Analyzer \
	BinaryInfo \
	BinarySearcher \
	Bookmarks \
	BreakpointManager \
	CheckVersion \
	DebuggerCore \
	DumpState \
	FunctionFinder \
	HardwareBreakpoints \
	OpcodeSearcher \
	ProcessProperties \
	ROPTool \
	References \
	SessionManager \
	StringSearcher \
	SymbolViewer

unix {
	!macx {
		SUBDIRS += HeapAnalyzer
	}

	linux-* {
		SUBDIRS += OpenFiles 
	}
}
