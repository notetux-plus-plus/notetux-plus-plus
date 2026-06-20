# The Notetux++ Manifesto

## Why this exists

I have been a Notepad++ user for as long as I can remember. It is one of those rare pieces of software that simply works — fast, lightweight, extensible, honest. No subscriptions. No telemetry. No bloat. Just a text editor that respects your time and your machine. Don Ho built something genuinely remarkable, and for that he deserves every bit of recognition he has received over the years.

But Notepad++ has always been a Windows application. Exclusively, stubbornly, almost defiantly so. For years — arguably for over a decade — developers on Linux and macOS have asked, politely and repeatedly, for a native port. The answer has always been silence, or a shrug, or a suggestion to use Wine. The community's love for the project was met with indifference toward half the people expressing it.

That indifference is not a crime. Maintainers owe nobody anything. But it does leave a gap, and gaps get filled.

---

## Andrey filled the gap first

Andrey Letov built a native macOS port of Notepad++. He called it *Notepad++ for Mac*. He used the same icon, the same name with a small qualifier, the same spirit. He did this because he loved the project and wanted to bring it to a platform it had never officially reached. He did it in the open, under the same licence that governs the original.

What he received in return was a cease-and-desist.

Don Ho — who has publicly praised the GPL licence as one of the most liberal and open licences in existence, who has spoken warmly about the values of free software — decided that a port, built out of admiration, distributed for free, carrying the project's name to a platform he had never bothered to support himself, was a threat worth fighting legally.

The stated justification was user confusion. The real effect was to punish someone for caring.

I disagree with that decision completely. Not bitterly, not with anger — but clearly and without reservation. The reasoning does not hold up, and the action was disproportionate to any conceivable harm.

---

## The missed opportunity

Here is the thought that lingers:

If the energy spent on legal threats had been spent on a conversation instead, there might today be an official, cross-platform Notepad++ — built collaboratively by the people who love it most. Andrey on macOS. Someone else on Linux. Don Ho steering the vision. The same application, everywhere, maintained by a community rather than a single developer on a single platform.

That did not happen. Instead, the community fragmented precisely because one part of it was pushed away.

Open source works best when it compounds. When contributors build on each other's work, credit each other honestly, and expand the reach of something good rather than contracting it. The Notepad++ ecosystem chose contraction. This project chooses the other path.

---

## Where Notetux++ comes from

Notetux++ is a fork of Andrey's macOS port, rewritten for Linux with GTK3 in C11. It inherits the vendored Scintilla and Lexilla libraries, the XML configuration format, the feature philosophy, and the overall vision of what a serious text editor should feel like on a Unix desktop.

It exists because:

- Linux deserved a native Notepad++-class editor, and waiting for an official one was no longer a reasonable strategy.
- Andrey's work proved it was possible and provided the foundation.
- The values that make Notepad++ worth using — speed, simplicity, respect for the user — are not Windows-specific values. They belong to every platform.

The name Notetux++ is a deliberate step away from the original trademark. Not because the original name is unworthy, but because this project is its own thing now, with its own roadmap, its own community, and its own identity. Tux, the Linux penguin, belongs in the name. The `++` stays, as a nod to where all of this began.

---

## What this project stands for

**Collaboration over litigation.** If you are building something in this space — a port, a plugin, a compatible tool — I want to talk to you, not send you a letter.

**Credit where it is due.** Don Ho built Notepad++. Andrey Letov proved it could live on macOS. Neil Hodgson built Scintilla and Lexilla. This project stands on all of their work and says so plainly.

**Native over emulated.** Wine is a remarkable technical achievement. It is not a substitute for a native application. Linux users deserve software that belongs on their platform, not software that tolerates it.

**Open, always.** Notetux++ is free software. Its source is open. Its future is shaped by the people who use and contribute to it. No exceptions.

---

---

## On C, and why this project will be written entirely in it

I love C.

Not in the nostalgic, vaguely ironic way that some programmers claim to love COBOL or Assembly — as a curiosity, a museum piece, a badge of survival. I love C because it is the most honest programming language ever designed. What you write is what the machine does. There is no runtime reaching behind your back, no object system quietly allocating memory you did not ask for, no template machinery expanding into forty screenfuls of code so that a single `.push_back()` can compile. C gives you a flat address space, some arithmetic, a handful of control-flow primitives, and nothing else. Everything that happens is something you chose.

C++ is the opposite philosophy. It is a language that tries to be ten languages at once and succeeds at none of them completely. Every C++ codebase carries with it an invisible weight: the weight of the features you chose *not* to use, but which your colleagues might, which the library you depend on does, which a new compiler flag might quietly enable. The language does not have a philosophy — it has a committee. And committees do not build beautiful things. They build things that have something for everyone, which is another way of saying things that are fully understood by no one.

Notetux++ was born as a C11 application. The UI layer is C. The configuration layer is C. The plugin API is C. But at the very core of the editor — the editing engine (Scintilla) and the syntax-highlighting library (Lexilla) — there has always been C++. We call it via a thin wrapper, `lexilla_bridge.cpp`, which exists for the sole reason that the Lexilla API cannot be reached without a C++ translation unit. That wrapper has always bothered me.

On the `c-conversion` branch, that botheration becomes a project.

The goal is to translate every `.cxx` file in Scintilla and Lexilla — all 34 Scintilla source files, 12 Lexilla library files, 1 Lexilla entry point, and 128 lexers — into clean, idiomatic C11. No C++ at all. Not a single translation unit. The `lexilla_bridge.cpp` wrapper will be deleted, not replaced. The result will be a single, self-contained C project that compiles with any C11-compliant compiler on any platform that has a C toolchain and a GTK3 installation.

This is not purely an educational exercise, though it begins as one. It is the path to a production goal: an editor that is blazingly fast because it wastes nothing on object machinery, that is portable because it carries no C++ ABI fragility, that is maintainable because any programmer who knows C — and that is most programmers, everywhere, at every level — can read and modify it without first learning what `std::enable_if_t` is or why `decltype(auto)` exists.

C is fragile in exactly one way: memory. Get the ownership wrong, and the program crashes, corrupts, or silently misbehaves. C++ offers to protect you from this fragility by wrapping everything in constructors and destructors and smart pointers and RAII. I accept the tradeoff in the other direction. I would rather own the memory explicitly, reason about it clearly, and write correct code intentionally — than delegate that responsibility to a runtime I do not fully control and trust that a `shared_ptr` reference cycle will not bite me in production at 3am.

The discipline required to write correct C is the same discipline required to write correct software in any language. C just refuses to let you pretend otherwise.

So: every `.cxx` becomes a `.c`. Every class becomes a struct with a vtable. Every template becomes an X-macro or a set of concrete type-specific functions. Every `std::string` becomes a `char *` and a length. Every `std::vector` becomes a heap array and a count. Every `throw` becomes an `assert` and an `abort`. The logic does not change — only the language it is written in.

It will take as long as it takes. There are about 180 files. Some of them are thousands of lines long. Document.cxx alone is the size of a small novel, and Editor.cxx is larger. The 128 lexers are each their own micro-project. But each file translated is a file that no longer needs a C++ compiler to build. And when the last one is done, `lexilla_bridge.cpp` will be deleted and nothing in this repository will ever include a `.cpp` file again.

That is the goal. That is why this branch exists.

*— Andrea Coi, 2026*
