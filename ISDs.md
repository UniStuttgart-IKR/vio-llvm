# ISDs: Symbole, Adressen, Konstanten, etc.

## ISD-Liste

- GlobalAddress
- GlobalTLSAddress
- JumpTable
- ConstantPool
- ExternalSymbol

### GlobalAddress

- Sinn: Materialisieren von Adressen globaler Variablen oder Funktionen
- Auftreten: 
- Operanden: 
- Lowering: Laden eines Pointers aus `ctxt` (globale Variablen), Laden eines Index der sich auf das aktuelle Codeobjekt bezieht (Funktionen)
- Sonstiges: Kann zu GlobalAddressSDNode gecastet werden, hat dann Methoden getGlobal (liefert GlobalValue) und getOffset (liefert int64_t)

### GlobalTLSAddress

- Sinn: Materialisieren von Adressen von Thread-Local Variablen
- Auftreten:
- Operanden:
- Lowering: Aktuell nicht nötig, da nur mit Multithreading sinnvoll, und noch kein Konzept für Thread Local Storage existiert

### JumpTable

- Sinn: Erzeugen eines Zeiges auf eine Sprungtabelle
- Auftreten: Bei Switch-Case-Blöcken mit 4 oder mehr Cases
- Operanden: ?
- Lowering: Laden eines Pointers aus `ctxt`
- Sonstiges: Crasht aktuell weil ```clang++: /u/bulk/home/stud/leylknci/vio-llvm/llvm/lib/CodeGen/SelectionDAG/SelectionDAG.cpp:7138: llvm::SDValue llvm::SelectionDAG::getNode(unsigned int, const llvm::SDLoc&, llvm::EVT, llvm::SDValue, llvm::SDNodeFlags): Assertion `VT.isInteger() && N1.getValueType().isInteger() && "Invalid ZERO_EXTEND!"' failed.```

### ConstantPool

- Sinn:
- Auftreten:
- Operanden:
- Lowering:
- Sonstiges:

### ExternalSymbol

- Sinn: Platzhalter, der vom Linker durch einen konkreten Wert ersetzt werden muss
- Auftreten: Wenn man irgendwas benutzt, was deklariert, aber nicht definiert ist
- Operanden: Keine
- Lowering: Einfügen des Symbols, Symbol irgendwo importieren
- Sonstiges: Sollte bei 🍊-RISC eigentlich nur bei jlibs auftreten
