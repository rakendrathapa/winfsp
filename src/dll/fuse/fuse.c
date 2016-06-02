/**
 * @file dll/fuse/fuse.c
 *
 * @copyright 2015-2016 Bill Zissimopoulos
 */
/*
 * This file is part of WinFsp.
 *
 * You can redistribute it and/or modify it under the terms of the
 * GNU Affero General Public License version 3 as published by the
 * Free Software Foundation.
 *
 * Licensees holding a valid commercial license may use this file in
 * accordance with the commercial license agreement provided with the
 * software.
 */

#include <dll/fuse/library.h>

#define FSP_FUSE_CORE_OPT(n, f, v)      { n, offsetof(struct fsp_fuse_core_opt_data, f), v }

struct fsp_fuse_core_opt_data
{
    struct fsp_fuse_env *env;
    int help, debug;
    int hard_remove,
        use_ino, readdir_ino,
        set_umask, umask,
        set_uid, uid,
        set_gid, gid,
        set_attr_timeout, attr_timeout;
    FILETIME VolumeCreationTime;
    int set_FileInfoTimeout;
    int CaseInsensitiveSearch, PersistentAcls,
        ReparsePoints, NamedStreams,
        ReadOnlyVolume;
    FSP_FSCTL_VOLUME_PARAMS VolumeParams;
};

static struct fuse_opt fsp_fuse_core_opts[] =
{
    FUSE_OPT_KEY("-h", 'h'),
    FUSE_OPT_KEY("--help", 'h'),
    FUSE_OPT_KEY("-V", 'V'),
    FUSE_OPT_KEY("--version", 'V'),
    FUSE_OPT_KEY("-d", FUSE_OPT_KEY_KEEP),
    FUSE_OPT_KEY("debug", FUSE_OPT_KEY_KEEP),
    FSP_FUSE_CORE_OPT("-d", debug, 1),
    FSP_FUSE_CORE_OPT("debug", debug, 1),

    FSP_FUSE_CORE_OPT("hard_remove", hard_remove, 1),
    FSP_FUSE_CORE_OPT("use_ino", use_ino, 1),
    FSP_FUSE_CORE_OPT("readdir_ino", readdir_ino, 1),
    FUSE_OPT_KEY("direct_io", FUSE_OPT_KEY_DISCARD),
    FUSE_OPT_KEY("kernel_cache", FUSE_OPT_KEY_DISCARD),
    FUSE_OPT_KEY("auto_cache", FUSE_OPT_KEY_DISCARD),
    FUSE_OPT_KEY("noauto_cache", FUSE_OPT_KEY_DISCARD),
    FSP_FUSE_CORE_OPT("umask=", set_umask, 1),
    FSP_FUSE_CORE_OPT("umask=%o", umask, 0),
    FSP_FUSE_CORE_OPT("uid=", set_uid, 1),
    FSP_FUSE_CORE_OPT("uid=%d", uid, 0),
    FSP_FUSE_CORE_OPT("gid=", set_gid, 1),
    FSP_FUSE_CORE_OPT("gid=%d", gid, 0),
    FUSE_OPT_KEY("entry_timeout", FUSE_OPT_KEY_DISCARD),
    FSP_FUSE_CORE_OPT("attr_timeout=", set_attr_timeout, 1),
    FSP_FUSE_CORE_OPT("attr_timeout=%d", attr_timeout, 0),
    FUSE_OPT_KEY("ac_attr_timeout", FUSE_OPT_KEY_DISCARD),
    FUSE_OPT_KEY("negative_timeout", FUSE_OPT_KEY_DISCARD),
    FUSE_OPT_KEY("noforget", FUSE_OPT_KEY_DISCARD),
    FUSE_OPT_KEY("intr", FUSE_OPT_KEY_DISCARD),
    FUSE_OPT_KEY("intr_signal=", FUSE_OPT_KEY_DISCARD),
    FUSE_OPT_KEY("modules=", FUSE_OPT_KEY_DISCARD),

    FSP_FUSE_CORE_OPT("SectorSize=%hu", VolumeParams.SectorSize, 4096),
    FSP_FUSE_CORE_OPT("SectorsPerAllocationUnit=%hu", VolumeParams.SectorsPerAllocationUnit, 1),
    FSP_FUSE_CORE_OPT("MaxComponentLength=%hu", VolumeParams.MaxComponentLength, 0),
    FSP_FUSE_CORE_OPT("VolumeCreationTime=%llx", VolumeCreationTime, 0),
    FSP_FUSE_CORE_OPT("VolumeCreationTimeLo=%x", VolumeCreationTime.dwLowDateTime, 0),
    FSP_FUSE_CORE_OPT("VolumeCreationTimeHi=%x", VolumeCreationTime.dwHighDateTime, 0),
    FSP_FUSE_CORE_OPT("VolumeSerialNumber=%lx", VolumeParams.VolumeSerialNumber, 0),
    FSP_FUSE_CORE_OPT("TransactTimeout=%u", VolumeParams.TransactTimeout, 0),
    FSP_FUSE_CORE_OPT("IrpTimeout=%u", VolumeParams.IrpTimeout, 0),
    FSP_FUSE_CORE_OPT("IrpCapacity=%u", VolumeParams.IrpCapacity, 0),
    FSP_FUSE_CORE_OPT("FileInfoTimeout=", set_FileInfoTimeout, 1),
    FSP_FUSE_CORE_OPT("FileInfoTimeout=%d", VolumeParams.FileInfoTimeout, 0),
    FSP_FUSE_CORE_OPT("CaseInsensitiveSearch", CaseInsensitiveSearch, 1),
    FSP_FUSE_CORE_OPT("PersistentAcls", PersistentAcls, 1),
    FSP_FUSE_CORE_OPT("ReparsePoints", ReparsePoints, 1),
    FSP_FUSE_CORE_OPT("NamedStreams", NamedStreams, 1),
    FUSE_OPT_KEY("HardLinks", FUSE_OPT_KEY_DISCARD),
    FUSE_OPT_KEY("ExtendedAttributes", FUSE_OPT_KEY_DISCARD),
    FSP_FUSE_CORE_OPT("ReadOnlyVolume", ReadOnlyVolume, 1),
    FUSE_OPT_KEY("--UNC=", 'U'),
    FUSE_OPT_KEY("--VolumePrefix=", 'U'),

    FUSE_OPT_END,
};

struct fuse_chan
{
    PWSTR MountPoint;
    UINT8 Buffer[];
};

struct fuse
{
    FSP_FILE_SYSTEM *FileSystem;
    FSP_SERVICE *Service;
    struct fuse_operations ops;
    void *data;
    int environment;
    CRITICAL_SECTION Lock;
};

static DWORD fsp_fuse_tlskey = TLS_OUT_OF_INDEXES;
static INIT_ONCE fsp_fuse_initonce_v = INIT_ONCE_STATIC_INIT;

VOID fsp_fuse_initialize(BOOLEAN Dynamic)
{
}

VOID fsp_fuse_finalize(BOOLEAN Dynamic)
{
    /*
     * This function is called during DLL_PROCESS_DETACH. We must therefore keep
     * finalization tasks to a minimum.
     *
     * We must free our TLS key (if any). We only do so if the library
     * is being explicitly unloaded (rather than the process exiting).
     */

    if (Dynamic && TLS_OUT_OF_INDEXES != fsp_fuse_tlskey)
    {
        /* !!!:
         * We should also free all thread local contexts, which means putting them in a list,
         * protected with a critical section, etc. Arghhh!
         *
         * I am too lazy and I am not going to do that, unless people start using this
         * DLL dynamically (LoadLibrary/FreeLibrary).
         */
        TlsFree(fsp_fuse_tlskey);
    }
}

VOID fsp_fuse_finalize_thread(VOID)
{
    struct fuse_context *context;

    if (TLS_OUT_OF_INDEXES != fsp_fuse_tlskey)
    {
        context = TlsGetValue(fsp_fuse_tlskey);
        if (0 != context)
        {
            MemFree(context);
            TlsSetValue(fsp_fuse_tlskey, 0);
        }
    }
}

static BOOL WINAPI fsp_fuse_initonce_f(
    PINIT_ONCE InitOnce, PVOID Parameter, PVOID *Context)
{
    fsp_fuse_tlskey = TlsAlloc();
    return TRUE;
}

static inline VOID fsp_fuse_initonce(VOID)
{
    InitOnceExecuteOnce(&fsp_fuse_initonce_v, fsp_fuse_initonce_f, 0, 0);
}

FSP_FUSE_API int fsp_fuse_version(struct fsp_fuse_env *env)
{
    return FUSE_VERSION;
}

FSP_FUSE_API struct fuse_chan *fsp_fuse_mount(struct fsp_fuse_env *env,
    const char *mountpoint, struct fuse_args *args)
{
    struct fuse_chan *ch = 0;
    int Size;

    if (0 == mountpoint)
        mountpoint = "";

    Size = MultiByteToWideChar(CP_UTF8, 0, mountpoint, -1, 0, 0);
    if (0 == Size)
        goto fail;

    ch = MemAlloc(sizeof *ch + Size * sizeof(WCHAR));
    if (0 == ch)
        goto fail;

    ch->MountPoint = (PVOID)ch->Buffer;
    Size = MultiByteToWideChar(CP_UTF8, 0, mountpoint, -1, ch->MountPoint, Size);
    if (0 == Size)
        goto fail;

    return ch;

fail:
    MemFree(ch);

    return 0;
}

FSP_FUSE_API void fsp_fuse_unmount(struct fsp_fuse_env *env,
    const char *mountpoint, struct fuse_chan *ch)
{
    MemFree(ch);
}

FSP_FUSE_API int fsp_fuse_is_lib_option(struct fsp_fuse_env *env,
    const char *opt)
{
    return fsp_fuse_opt_match(env, fsp_fuse_core_opts, opt);
}

static NTSTATUS fsp_fuse_svcstart(FSP_SERVICE *Service, ULONG argc, PWSTR *argv)
{
    struct fuse *f = Service->UserContext;

    return FspFileSystemStartDispatcher(f->FileSystem, 0);
}

static NTSTATUS fsp_fuse_svcstop(FSP_SERVICE *Service)
{
    struct fuse *f = Service->UserContext;

    FspFileSystemStopDispatcher(f->FileSystem);

    return STATUS_SUCCESS;
}

static int fsp_fuse_core_opt_proc(void *opt_data0, const char *arg, int key,
    struct fuse_args *outargs)
{
    struct fsp_fuse_core_opt_data *opt_data = opt_data0;

    switch (key)
    {
    default:
        return 1;
    case 'h':
        FspServiceLog(EVENTLOG_ERROR_TYPE, L""
            LIBRARY_NAME "-FUSE options:\n"
            "    -o SectorSize=N        sector size for Windows (512-4096, deflt: 4096)\n"
            "    -o SectorsPerAllocationUnit=N  allocation unit size (deflt: 1*SectorSize)\n"
            "    -o MaxComponentLength=N    max file name component length (deflt: 255)\n"
            "    -o VolumeCreationTime=T    volume creation time (FILETIME hex format)\n"
            "    -o VolumeSerialNumber=N    32-bit wide\n"
            "    -o FileInfoTimeout=N       FileInfo/Security/VolumeInfo timeout (millisec)\n"
            "    -o CaseInsensitiveSearch   file system supports case-insensitive file names\n"
            "    -o PersistentAcls          file system preserves and enforces ACL's\n"
            //"    -o ReparsePoints           file system supports reparse points\n"
            //"    -o NamedStreams            file system supports named streams\n"
            //"    -o ReadOnlyVolume          file system is read only\n"
            "    --UNC=U --VolumePrefix=U   UNC prefix (\\Server\\Share)\n");
        opt_data->help = 1;
        return 1;
    case 'V':
        FspServiceLog(EVENTLOG_ERROR_TYPE, L""
            LIBRARY_NAME "-FUSE version %d.%d",
            FUSE_MAJOR_VERSION, FUSE_MINOR_VERSION);
        opt_data->help = 1;
        return 1;
    case 'U':
        if ('U' == arg[2])
            arg += sizeof "--UNC" - 1;
        else if ('V' == arg[2])
            arg += sizeof "--VolumePrefix" - 1;
        if (0 == MultiByteToWideChar(CP_UTF8, 0, arg, -1,
            opt_data->VolumeParams.Prefix, sizeof opt_data->VolumeParams.Prefix / sizeof(WCHAR)))
            return -1;
        return 0;
    }
}

FSP_FUSE_API struct fuse *fsp_fuse_new(struct fsp_fuse_env *env,
    struct fuse_chan *ch, struct fuse_args *args,
    const struct fuse_operations *ops, size_t opsize, void *data)
{
    static FSP_FILE_SYSTEM_INTERFACE intf =
    {
        0,
        0,
        fsp_fuse_op_get_security_by_name,
    };
    struct fsp_fuse_core_opt_data opt_data;
    struct fuse_conn_info conn;
    struct fuse_context *context;
    struct fuse *f = 0;
    PWSTR ServiceName = FspDiagIdent();
    PWSTR ErrorMessage = L".";
    NTSTATUS Result;
    BOOLEAN FsInit = FALSE;

    memset(&opt_data, 0, sizeof opt_data);
    opt_data.env = env;

    if (-1 == fsp_fuse_opt_parse(env, args, &opt_data, fsp_fuse_core_opts, fsp_fuse_core_opt_proc))
        return 0;
    if (opt_data.help)
        return 0;

    opt_data.VolumeParams.VolumeCreationTime = *(PUINT64)&opt_data.VolumeCreationTime;
    if (0 == opt_data.VolumeParams.VolumeCreationTime)
    {
        FILETIME FileTime;
        GetSystemTimeAsFileTime(&FileTime);
        opt_data.VolumeParams.VolumeCreationTime = *(PUINT64)&FileTime;
    }
    if (0 == opt_data.VolumeParams.VolumeSerialNumber)
        opt_data.VolumeParams.VolumeSerialNumber =
            ((PLARGE_INTEGER)&opt_data.VolumeParams.VolumeCreationTime)->HighPart ^
            ((PLARGE_INTEGER)&opt_data.VolumeParams.VolumeCreationTime)->LowPart;
    if (!opt_data.set_FileInfoTimeout && opt_data.set_attr_timeout)
        opt_data.VolumeParams.FileInfoTimeout = opt_data.set_attr_timeout * 1000;
    opt_data.VolumeParams.CaseSensitiveSearch = !opt_data.CaseInsensitiveSearch;
    opt_data.VolumeParams.PersistentAcls = !!opt_data.PersistentAcls;
    opt_data.VolumeParams.ReparsePoints = !!opt_data.ReparsePoints;
    opt_data.VolumeParams.NamedStreams = !!opt_data.NamedStreams;
    opt_data.VolumeParams.ReadOnlyVolume = !!opt_data.ReadOnlyVolume;

    context = fsp_fuse_get_context(env);
    if (0 == context)
        goto fail;
    context->private_data = data;

    memset(&conn, 0, sizeof conn);
    conn.proto_major = 7;               /* pretend that we are FUSE kernel protocol 7.12 */
    conn.proto_minor = 12;              /*     which was current at the time of FUSE 2.8 */
    conn.async_read = 1;
    conn.max_write = UINT_MAX;
    conn.capable =
        FUSE_CAP_ASYNC_READ |
        //FUSE_CAP_POSIX_LOCKS |        /* WinFsp handles locking in the FSD currently */
        //FUSE_CAP_ATOMIC_O_TRUNC |     /* due to Windows/WinFsp design, no support */
        //FUSE_CAP_EXPORT_SUPPORT |     /* not needed in Windows/WinFsp */
        FUSE_CAP_BIG_WRITES |
        FUSE_CAP_DONT_MASK;
    if (0 != ops->init)
        context->private_data = ops->init(&conn);
    FsInit = TRUE;
    if (0 != ops->statfs)
    {
        union
        {
            struct cyg_statvfs cygbuf;
            struct fuse_statvfs fspbuf;
        } buf;
        struct fuse_statvfs fspbuf;
        int err;

        err = ops->statfs("/", (void *)&buf);
        if (0 != err)
            goto fail;
        fsp_fuse_op_get_statvfs_buf(env->environment, &buf, &fspbuf);

        opt_data.VolumeParams.SectorSize = (UINT16)fspbuf.f_bsize;
        opt_data.VolumeParams.MaxComponentLength = (UINT16)fspbuf.f_namemax;
    }

    /* !!!: the FSD does not currently limit the VolumeParams fields! */

    f = MemAlloc(sizeof *f);
    if (0 == f)
        goto fail;
    memset(f, 0, sizeof *f);

    Result = FspServiceCreate(ServiceName, fsp_fuse_svcstart, fsp_fuse_svcstop, 0, &f->Service);
    if (!NT_SUCCESS(Result))
        goto fail;
    FspServiceAllowConsoleMode(f->Service);
    f->Service->UserContext = f;

    Result = FspFileSystemCreate(L"" FSP_FSCTL_NET_DEVICE_NAME, &opt_data.VolumeParams, &intf,
        &f->FileSystem);
    if (!NT_SUCCESS(Result))
    {
        ErrorMessage = L": cannot create " LIBRARY_NAME " file system object.";
        goto fail;
    }

    FspFileSystemSetOperation(f->FileSystem, FspFsctlTransactCreateKind,
        fsp_fuse_op_create);
    FspFileSystemSetOperation(f->FileSystem, FspFsctlTransactOverwriteKind,
        fsp_fuse_op_overwrite);
    FspFileSystemSetOperation(f->FileSystem, FspFsctlTransactCleanupKind,
        fsp_fuse_op_cleanup);
    FspFileSystemSetOperation(f->FileSystem, FspFsctlTransactCloseKind,
        fsp_fuse_op_close);
    FspFileSystemSetOperation(f->FileSystem, FspFsctlTransactReadKind,
        fsp_fuse_op_read);
    FspFileSystemSetOperation(f->FileSystem, FspFsctlTransactWriteKind,
        fsp_fuse_op_write);
    FspFileSystemSetOperation(f->FileSystem, FspFsctlTransactQueryInformationKind,
        fsp_fuse_op_query_information);
    FspFileSystemSetOperation(f->FileSystem, FspFsctlTransactSetInformationKind,
        fsp_fuse_op_set_information);
    FspFileSystemSetOperation(f->FileSystem, FspFsctlTransactFlushBuffersKind,
        fsp_fuse_op_flush_buffers);
    FspFileSystemSetOperation(f->FileSystem, FspFsctlTransactQueryVolumeInformationKind,
        fsp_fuse_op_query_volume_information);
    FspFileSystemSetOperation(f->FileSystem, FspFsctlTransactSetVolumeInformationKind,
        fsp_fuse_op_set_volume_information);
    FspFileSystemSetOperation(f->FileSystem, FspFsctlTransactQueryDirectoryKind,
        fsp_fuse_op_query_directory);
    FspFileSystemSetOperation(f->FileSystem, FspFsctlTransactQuerySecurityKind,
        fsp_fuse_op_query_security);
    FspFileSystemSetOperation(f->FileSystem, FspFsctlTransactSetSecurityKind,
        fsp_fuse_op_set_security);
    //FspFileSystemSetOperationGuard(f->FileSystem, 0, 0);

    if (opt_data.debug)
        FspFileSystemSetDebugLog(f->FileSystem, -1);

    if (L'\0' != ch->MountPoint)
    {
        Result = FspFileSystemSetMountPoint(f->FileSystem,
            L'*' == ch->MountPoint[0] && L'\0' == ch->MountPoint[1] ? 0 : ch->MountPoint);
        if (!NT_SUCCESS(Result))
        {
            ErrorMessage = L": cannot set mount point.";
            goto fail;
        }
    }

    memcpy(&f->ops, ops, opsize);
    f->data = context->private_data;
    f->environment = env->environment;
    InitializeCriticalSection(&f->Lock);

    context->fuse = f;

    return f;

fail:
    FspServiceLog(EVENTLOG_ERROR_TYPE, L"Unable to create FUSE file system%s");

    if (0 != f)
    {
        if (0 != f->FileSystem)
            FspFileSystemDelete(f->FileSystem);

        if (0 != f->Service)
            FspServiceDelete(f->Service);

        MemFree(f);
    }

    if (FsInit)
    {
        if (0 != ops->destroy)
            ops->destroy(context->private_data);
    }

    return 0;
}

FSP_FUSE_API void fsp_fuse_destroy(struct fsp_fuse_env *env,
    struct fuse *f)
{
    DeleteCriticalSection(&f->Lock);

    FspFileSystemDelete(f->FileSystem);

    FspServiceDelete(f->Service);

    MemFree(f);
}

FSP_FUSE_API int fsp_fuse_loop(struct fsp_fuse_env *env,
    struct fuse *f)
{
    NTSTATUS Result;
    ULONG ExitCode;

    Result = FspServiceLoop(f->Service);
    ExitCode = FspServiceGetExitCode(f->Service);

    if (!NT_SUCCESS(Result))
        goto fail;

    if (0 != ExitCode)
        FspServiceLog(EVENTLOG_WARNING_TYPE,
            L"The service %s has exited (ExitCode=%ld).", f->Service->ServiceName, ExitCode);

    return 0;

fail:
    FspServiceLog(EVENTLOG_ERROR_TYPE,
        L"The service %s has failed to run (Status=%lx).", f->Service->ServiceName, Result);

    return -1;
}

FSP_FUSE_API int fsp_fuse_loop_mt(struct fsp_fuse_env *env,
    struct fuse *f)
{
    return fsp_fuse_loop(env, f);
}

FSP_FUSE_API void fsp_fuse_exit(struct fsp_fuse_env *env,
    struct fuse *f)
{
    FspServiceStop(f->Service);
}

FSP_FUSE_API struct fuse_context *fsp_fuse_get_context(struct fsp_fuse_env *env)
{
    struct fuse_context *context;

    fsp_fuse_initonce();
    if (TLS_OUT_OF_INDEXES == fsp_fuse_tlskey)
        return 0;

    context = TlsGetValue(fsp_fuse_tlskey);
    if (0 == context)
    {
        context = MemAlloc(sizeof *context);
        if (0 == context)
            return 0;

        memset(context, 0, sizeof *context);

        TlsSetValue(fsp_fuse_tlskey, context);
    }

    return context;
}