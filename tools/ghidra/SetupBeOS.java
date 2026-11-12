// Pre-analysis setup for BeOS R5 x86 (gcc2) binaries: demangle with the old
// GNU v2 scheme so the exported symbols become classes and methods.
//@category BeOS

import ghidra.app.script.GhidraScript;

public class SetupBeOS extends GhidraScript {
	@Override
	protected void run() throws Exception {
		setAnalysisOption(currentProgram, "Demangler GNU.Demangler Format", "GNU");
		setAnalysisOption(currentProgram, "Demangler GNU.Use Deprecated Demangler", "true");
		setAnalysisOption(currentProgram, "Demangler GNU.Apply Function Signatures", "true");
		println("BeOS setup: GNU v2 demangling enabled for " + currentProgram.getName());
	}
}
