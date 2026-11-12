// Replace the empty placeholder structs the demangler creates for Be API
// classes (BRect, BMessage, ...) with the real types from BeAPI_gcc2.gdt
// (see tools/betypes/build.sh). Function signatures that use a placeholder
// pick up the real layout.
//
// If GobeApp.gdt exists (the main app's classes, saved by RecoverClasses), its
// classes fill the placeholders too, so the parts and translators use the main
// app's layouts for classes like TPart. Be API types take precedence.
//
// Run after analysis (the demangler creates the placeholders) and before
// FixThiscall, which needs argument sizes.
//@category BeOS

import java.io.File;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.Set;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.data.*;

public class ApplyBeTypes extends GhidraScript {
	@Override
	protected void run() throws Exception {
		File dir = getSourceFile().getParentFile().getFile(false);
		List<FileDataTypeManager> archives = new ArrayList<>();
		archives.add(FileDataTypeManager.openFileArchive(new File(dir, "BeAPI_gcc2.gdt"), false));
		File app = new File(dir, "GobeApp.gdt");
		if (app.exists() && !currentProgram.getName().equals("Gobe Productive"))
			archives.add(FileDataTypeManager.openFileArchive(app, false));
		try {
			Map<String, Structure> be = new HashMap<>();
			Set<String> ambiguous = new HashSet<>();
			for (FileDataTypeManager archive : archives) {
				// an earlier archive wins; a name defined twice in this archive
				// (and not supplied earlier) is ambiguous
				Map<String, Structure> mine = new HashMap<>();
				Set<String> dupes = new HashSet<>();
				Iterator<Structure> it = archive.getAllStructures();
				while (it.hasNext()) {
					Structure s = it.next();
					if (s.isNotYetDefined() || s.getLength() <= 1)
						continue;
					if (mine.put(s.getName(), s) != null)
						dupes.add(s.getName());
				}
				for (Map.Entry<String, Structure> e : mine.entrySet()) {
					if (be.containsKey(e.getKey()))
						continue;
					if (dupes.contains(e.getKey()))
						ambiguous.add(e.getKey());
					else
						be.put(e.getKey(), e.getValue());
				}
			}

			DataTypeManager dtm = currentProgram.getDataTypeManager();
			List<Structure> placeholders = new ArrayList<>();
			Iterator<Structure> pit = dtm.getAllStructures();
			while (pit.hasNext()) {
				Structure s = pit.next();
				if (s.getCategoryPath().getPath().startsWith("/BeAPI"))
					continue;
				if (!be.containsKey(s.getName()) || ambiguous.contains(s.getName()))
					continue;
				if (s.isNotYetDefined() || s.getLength() <= 1 || s.isZeroLength())
					placeholders.add(s);
			}

			int replaced = 0;
			List<String> names = new ArrayList<>();
			List<String> failed = new ArrayList<>();
			for (Structure old : placeholders) {
				try {
					DataType real = dtm.resolve(be.get(old.getName()),
						DataTypeConflictHandler.REPLACE_HANDLER);
					if (real == old || real == DataType.DEFAULT || real instanceof Undefined) {
						// resolve already replaced the placeholder in place
						if (!old.isNotYetDefined() && old.getLength() > 1) {
							replaced++;
							names.add(old.getName());
						}
						continue;
					}
					dtm.replaceDataType(old, real, true);
					replaced++;
					names.add(old.getName());
				} catch (Exception e) {
					failed.add(old.getName() + " (" + e.getMessage() + ")");
				}
			}
			if (!failed.isEmpty())
				println("could not replace: " + failed);
			java.util.Collections.sort(names);
			println(currentProgram.getName() + ": replaced " + replaced
				+ " placeholders: " + names);
		} finally {
			for (FileDataTypeManager archive : archives)
				archive.close();
		}
	}
}
