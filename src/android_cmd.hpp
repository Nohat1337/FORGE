#pragma once

// ============================================================
// forgevm android - build Android apps from Forge programs.
//
// The pipeline:
//
//   .fge  --(java backend)-->  ForgeMain.java
//        --(javac)-->         app module of a Gradle project
//        --(gradle)-->        app-debug.apk
//
// The generated app ships the Android liquid glass library
// (com.github.QWEA0:liquidglass), so every Forge widget is a real
// SDF glass lens on Android too.
//
// Commands:
//   forgevm android new <dir> [--package p] [--name N] [--source file.fge]
//   forgevm android build [dir]
//   forgevm android install [dir]
//   forgevm android run [dir]
//   forgevm android clean [dir]
// ============================================================

#include <string>
#include <vector>

namespace forge::androidcmd {

int android_main(int argc, char** argv);

} // namespace forge::androidcmd
