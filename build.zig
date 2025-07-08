const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.resolveTargetQuery(std.Target.Query{
        .cpu_arch = .x86,
        .os_tag = .linux,
        .abi = .gnu,
    });

    const optimize = b.standardOptimizeOption(.{ .preferred_optimize_mode = .ReleaseSmall });

    const subhook = b.addStaticLibrary(.{
        .name = "subhook",
        .target = target,
        .optimize = optimize,
    });

    subhook.linkLibC();

    subhook.addCSourceFiles(.{
        .files = &.{
            "Rewind/subhook/subhook.c",
            "Rewind/subhook/subhook_x86.c",
            "Rewind/subhook/subhook_unix.c",
        },
    });

    const jmp = b.addStaticLibrary(.{
        .name = "jmp",
        .target = target,
        .optimize = optimize,
    });

    jmp.linkLibC();
    jmp.linkLibCpp();

    jmp.addCSourceFiles(.{
        .files = &.{
            "Rewind/JMP/src/JMP/Platforms/Linux.cpp",
        },
        .flags = &.{
            "-std=c++20",
        },
    });

    const rewind = b.addSharedLibrary(.{
        .name = "rewind",
        .optimize = optimize,
        .target = target,
    });

    rewind.linkLibC();
    rewind.linkLibCpp();

    // FIXME: Supposedly fixed in master, writing as of 0.14.1
    rewind.link_z_notext = true;

    rewind.addCSourceFiles(.{
        .files = &.{
            "Rewind/src/PauseItRewind.cpp",
            "Rewind/sourcemod/public/smsdk_ext.cpp",
        },
        .flags = &.{
            "-std=c++20",
            "-DSOURCE_ENGINE=11",
            "-Dstricmp=strcasecmp",
            "-D_stricmp=strcasecmp",
            "-D_snprintf=snprintf",
            "-D_vsnprintf=vsnprintf",
            "-DGNUC",
            "-D_LINUX",
            "-DPOSIX",
            "-DNO_MALLOC_OVERRIDE",

            "-DSE_EPISODEONE=1",
            "-DSE_ORANGEBOX=3",
            "-DSE_CSS=6",
            "-DSE_HL2DM=7",
            "-DSE_DODS=8",
            "-DSE_SDK2013=9",
            "-DSE_LEFT4DEAD=12",
            "-DSE_NUCLEARDAWN=13",
            "-DSE_LEFT4DEAD2=15",
            "-DSE_DARKMESSIAH=2",
            "-DSE_ALIENSWARM=16",
            "-DSE_BLOODYGOODTIME=4",
            "-DSE_EYE=5",
            "-DSE_CSGO=21",
            "-DSE_PORTAL2=17",
            "-DSE_BLADE=18",
            "-DSE_INSURGENCY=19",
            "-DSE_CONTAGION=14",
            "-DSE_DOI=20",
            "-DSE_MOCK=999",
        },
    });

    // FIXME: why? for JMP and subhook maybe?
    rewind.addSystemIncludePath(b.path("Rewind"));
    rewind.addSystemIncludePath(b.path("Rewind/hl2sdk/common"));
    rewind.addSystemIncludePath(b.path("Rewind/hl2sdk/public"));
    rewind.addSystemIncludePath(b.path("Rewind/hl2sdk/public/mathlib"));
    rewind.addSystemIncludePath(b.path("Rewind/hl2sdk/public/vstdlib"));
    rewind.addSystemIncludePath(b.path("Rewind/hl2sdk/public/tier0"));
    rewind.addSystemIncludePath(b.path("Rewind/hl2sdk/public/tier1"));
    rewind.addSystemIncludePath(b.path("Rewind/hl2sdk/public/game/server"));
    rewind.addSystemIncludePath(b.path("Rewind/metamod-source/core"));
    rewind.addSystemIncludePath(b.path("Rewind/metamod-source/core/sourcehook"));

    rewind.addSystemIncludePath(b.path("Rewind/sourcemod"));
    rewind.addSystemIncludePath(b.path("Rewind/sourcemod/public"));
    rewind.addSystemIncludePath(b.path("Rewind/sourcemod/public/extensions"));
    rewind.addSystemIncludePath(b.path("Rewind/sourcemod/sourcepawn/include"));
    rewind.addSystemIncludePath(b.path("Rewind/sourcemod/public/amtl/amtl"));
    rewind.addSystemIncludePath(b.path("Rewind/sourcemod/public/amtl"));
    rewind.addSystemIncludePath(b.path("Rewind/sourcemod/bridge/include"));

    rewind.addSystemIncludePath(b.path("Rewind/JMP/src"));

    rewind.addSystemIncludePath(b.path("Rewind/src"));

    // NOTE: we don't normally say it but i think subhook does it automatically for us
    rewind.addSystemIncludePath(b.path("Rewind/subhook"));

    rewind.addLibraryPath(b.path("Rewind/hl2sdk/lib/linux"));
    rewind.linkSystemLibrary("tier0_srv");

    rewind.linkLibrary(subhook);
    rewind.linkLibrary(jmp);

    b.installArtifact(rewind);
}
