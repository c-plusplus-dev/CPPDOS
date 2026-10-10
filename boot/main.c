#ifndef OS_BOOT_MAIN_C
#define OS_BOOT_MAIN_C

#include <stdint.h>
#include <stddef.h>

/* =========================================================
   Basic UEFI types and constants
   ========================================================= */

typedef uint64_t UINTN;
typedef uint64_t EFI_STATUS;
typedef uint16_t CHAR16;
typedef void *EFI_HANDLE;
typedef uint64_t EFI_PHYSICAL_ADDRESS;

#define EFI_SUCCESS          0ULL
#define EFI_LOAD_ERROR       0x8000000000000001ULL
#define EFI_LOADER_CODE      1U
#define EFI_LOADER_DATA      2U
#define EFI_ALLOCATE_ADDRESS 2U

#define PAGE_SIZE            4096ULL
#define KERNEL_FILE_CAPACITY (16ULL * 1024ULL * 1024ULL)
#define MEMORY_MAP_CAPACITY  (1024ULL * 1024ULL)
#define KERNEL_MIN_ADDRESS   0x00100000ULL
#define KERNEL_MAX_ADDRESS   0x40000000ULL

typedef struct {
    uint64_t Signature;
    uint32_t Revision;
    uint32_t HeaderSize;
    uint32_t CRC32;
    uint32_t Reserved;
} EFI_TABLE_HEADER;

typedef struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;

typedef EFI_STATUS (*EFI_OUTPUT_STRING)(
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This,
    const CHAR16 *String
);

struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL {
    void *Reset;
    EFI_OUTPUT_STRING OutputString;
};

typedef struct {
    EFI_TABLE_HEADER Hdr;
    CHAR16 *FirmwareVendor;
    uint32_t FirmwareRevision;
    EFI_HANDLE ConsoleInHandle;
    void *ConIn;
    EFI_HANDLE ConsoleOutHandle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut;
    EFI_HANDLE StandardErrorHandle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *StdErr;
    void *RuntimeServices;
    void *BootServices;
    UINTN NumberOfTableEntries;
    void *ConfigurationTable;
} EFI_SYSTEM_TABLE;

/*
 * UEFI Boot Services function slots used here:
 *   2 AllocatePages
 *   4 GetMemoryMap
 *   5 AllocatePool
 *   6 FreePool
 *   26 ExitBootServices
 *   37 LocateProtocol
 */
typedef struct {
    EFI_TABLE_HEADER Hdr;
    void *Functions[44];
} EFI_BOOT_SERVICES;

typedef EFI_STATUS (*EFI_ALLOCATE_PAGES)(
    uint32_t Type, uint32_t MemoryType,
    UINTN Pages, EFI_PHYSICAL_ADDRESS *Memory
);
typedef EFI_STATUS (*EFI_GET_MEMORY_MAP)(
    UINTN *MemoryMapSize, void *MemoryMap, UINTN *MapKey,
    UINTN *DescriptorSize, uint32_t *DescriptorVersion
);
typedef EFI_STATUS (*EFI_ALLOCATE_POOL)(
    uint32_t PoolType, UINTN Size, void **Buffer
);
typedef EFI_STATUS (*EFI_FREE_POOL)(void *Buffer);
typedef EFI_STATUS (*EFI_EXIT_BOOT_SERVICES)(
    EFI_HANDLE ImageHandle, UINTN MapKey
);
typedef EFI_STATUS (*EFI_LOCATE_PROTOCOL)(
    const void *Protocol, void *Registration, void **Interface
);

typedef struct EFI_FILE_PROTOCOL EFI_FILE_PROTOCOL;
typedef EFI_STATUS (*EFI_FILE_OPEN)(
    EFI_FILE_PROTOCOL *This, EFI_FILE_PROTOCOL **NewHandle,
    const CHAR16 *FileName, uint64_t OpenMode, uint64_t Attributes
);
typedef EFI_STATUS (*EFI_FILE_CLOSE)(EFI_FILE_PROTOCOL *This);
typedef EFI_STATUS (*EFI_FILE_READ)(
    EFI_FILE_PROTOCOL *This, UINTN *BufferSize, void *Buffer
);

struct EFI_FILE_PROTOCOL {
    uint64_t Revision;
    EFI_FILE_OPEN Open;
    EFI_FILE_CLOSE Close;
    void *Delete;
    EFI_FILE_READ Read;
    void *Write;
    void *GetPosition;
    void *SetPosition;
    void *GetInfo;
    void *SetInfo;
    void *Flush;
};

typedef struct {
    uint64_t Revision;
    EFI_STATUS (*OpenVolume)(void *This, EFI_FILE_PROTOCOL **Root);
} EFI_SIMPLE_FILE_SYSTEM_PROTOCOL;

typedef struct {
    uint32_t Data1;
    uint16_t Data2;
    uint16_t Data3;
    uint8_t Data4[8];
} EFI_GUID;

/* =========================================================
   UEFI Graphics Output Protocol (GOP)
   ========================================================= */

typedef struct {
    uint32_t Version;
    uint32_t HorizontalResolution;
    uint32_t VerticalResolution;
    uint32_t PixelFormat;
    struct {
        uint32_t RedMask;
        uint32_t GreenMask;
        uint32_t BlueMask;
        uint32_t ReservedMask;
    } PixelInformation;
    uint32_t PixelsPerScanLine;
} EFI_GRAPHICS_OUTPUT_MODE_INFORMATION;

typedef struct {
    uint32_t MaxMode;
    uint32_t Mode;
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *Info;
    UINTN SizeOfInfo;
    EFI_PHYSICAL_ADDRESS FrameBufferBase;
    UINTN FrameBufferSize;
} EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE;

typedef struct {
    void *QueryMode;
    void *SetMode;
    void *Blt;
    EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE *Mode;
} EFI_GRAPHICS_OUTPUT_PROTOCOL;

/*
 * PixelFormat:
 *   0 = PixelRedGreenBlueReserved8BitPerColor (RGB bytes)
 *   1 = PixelBlueGreenRedReserved8BitPerColor (BGR bytes)
 *   2 = bit masks (not handled by this first framebuffer renderer)
 *   3 = BLT-only (no directly writable framebuffer)
 */
#define GOP_PIXEL_RGBX 0U
#define GOP_PIXEL_BGRX 1U

typedef struct {
    void *memory_map;
    uint64_t memory_map_size;
    uint64_t descriptor_size;
    uint32_t descriptor_version;

    uint64_t framebuffer_base;
    uint64_t framebuffer_size;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint32_t pixels_per_scanline;
    uint32_t framebuffer_pixel_format;
} BootInfo;

/* =========================================================
   ELF64 structures
   ========================================================= */

typedef struct {
    unsigned char ident[16];
    uint16_t type;
    uint16_t machine;
    uint32_t version;
    uint64_t entry;
    uint64_t program_header_offset;
    uint64_t section_header_offset;
    uint32_t flags;
    uint16_t header_size;
    uint16_t program_header_entry_size;
    uint16_t program_header_count;
    uint16_t section_header_entry_size;
    uint16_t section_header_count;
    uint16_t section_name_index;
} ELF64_HEADER;

typedef struct {
    uint32_t type;
    uint32_t flags;
    uint64_t offset;
    uint64_t virtual_address;
    uint64_t physical_address;
    uint64_t file_size;
    uint64_t memory_size;
    uint64_t alignment;
} ELF64_PROGRAM_HEADER;

#define ELF_ET_EXEC          2U
#define ELF_EM_X86_64        62U
#define ELF_VERSION_CURRENT  1U
#define ELF_PT_LOAD          1U
#define ELF_PF_X             1U

typedef void (__attribute__((sysv_abi)) *KERNEL_ENTRY)(BootInfo *BootInformation);

/* =========================================================
   Small helpers
   ========================================================= */

static EFI_STATUS fail(EFI_SYSTEM_TABLE *SystemTable, const CHAR16 *Message) {
    if (SystemTable != 0 &&
        SystemTable->ConOut != 0 &&
        SystemTable->ConOut->OutputString != 0) {
        SystemTable->ConOut->OutputString(SystemTable->ConOut, Message);
    }
    return EFI_LOAD_ERROR;
}

static EFI_STATUS allocate_pool(
    EFI_SYSTEM_TABLE *SystemTable, UINTN Size, void **Buffer
) {
    EFI_BOOT_SERVICES *bs = (EFI_BOOT_SERVICES *)SystemTable->BootServices;
    EFI_ALLOCATE_POOL AllocatePool = (EFI_ALLOCATE_POOL)bs->Functions[5];
    return AllocatePool(EFI_LOADER_DATA, Size, Buffer);
}

static EFI_STATUS free_pool(EFI_SYSTEM_TABLE *SystemTable, void *Buffer) {
    EFI_BOOT_SERVICES *bs = (EFI_BOOT_SERVICES *)SystemTable->BootServices;
    EFI_FREE_POOL FreePool = (EFI_FREE_POOL)bs->Functions[6];
    return FreePool(Buffer);
}

static void zero_memory(void *Destination, UINTN Size) {
    uint8_t *destination = (uint8_t *)Destination;
    for (UINTN i = 0; i < Size; ++i) destination[i] = 0;
}

static void copy_memory(void *Destination, const void *Source, UINTN Size) {
    uint8_t *destination = (uint8_t *)Destination;
    const uint8_t *source = (const uint8_t *)Source;
    for (UINTN i = 0; i < Size; ++i) destination[i] = source[i];
}

static int is_power_of_two(uint64_t value) {
    return value != 0 && (value & (value - 1)) == 0;
}

/* =========================================================
   Find the current UEFI framebuffer
   ========================================================= */

static void discover_framebuffer(
    EFI_SYSTEM_TABLE *SystemTable,
    BootInfo *boot_info
) {
    static const EFI_GUID gop_guid = {
        0x9042a9de, 0x23dc, 0x4a38,
        { 0x96, 0xfb, 0x7a, 0xde, 0xd0, 0x80, 0x51, 0x6a }
    };

    EFI_BOOT_SERVICES *bs = (EFI_BOOT_SERVICES *)SystemTable->BootServices;
    EFI_LOCATE_PROTOCOL LocateProtocol =
        (EFI_LOCATE_PROTOCOL)bs->Functions[37];

    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = 0;

    boot_info->framebuffer_base = 0;
    boot_info->framebuffer_size = 0;
    boot_info->framebuffer_width = 0;
    boot_info->framebuffer_height = 0;
    boot_info->pixels_per_scanline = 0;
    boot_info->framebuffer_pixel_format = UINT32_MAX;

    if (LocateProtocol(&gop_guid, 0, (void **)&gop) != EFI_SUCCESS ||
        gop == 0 || gop->Mode == 0 || gop->Mode->Info == 0) {
        return;
    }

    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *info = gop->Mode->Info;

    if (info->PixelFormat != GOP_PIXEL_RGBX &&
        info->PixelFormat != GOP_PIXEL_BGRX) {
        return;
    }

    if (info->HorizontalResolution == 0 ||
        info->VerticalResolution == 0 ||
        info->PixelsPerScanLine < info->HorizontalResolution) {
        return;
    }

    uint64_t required_size =
        (uint64_t)info->PixelsPerScanLine *
        (uint64_t)info->VerticalResolution * 4ULL;

    if (gop->Mode->FrameBufferBase == 0 ||
        gop->Mode->FrameBufferSize < required_size) {
        return;
    }

    boot_info->framebuffer_base = gop->Mode->FrameBufferBase;
    boot_info->framebuffer_size = gop->Mode->FrameBufferSize;
    boot_info->framebuffer_width = info->HorizontalResolution;
    boot_info->framebuffer_height = info->VerticalResolution;
    boot_info->pixels_per_scanline = info->PixelsPerScanLine;
    boot_info->framebuffer_pixel_format = info->PixelFormat;
}

/* =========================================================
   Load kernel.elf from the EFI System Partition
   ========================================================= */

static EFI_STATUS load_kernel(
    EFI_SYSTEM_TABLE *SystemTable,
    EFI_PHYSICAL_ADDRESS *KernelEntryAddress
) {
    static const CHAR16 error_fs[] = {
        'B','o','o','t','l','o','a','d','e','r',':',' ',
        'f','i','l','e','s','y','s','t','e','m',' ',
        'u','n','a','v','a','i','l','a','b','l','e','.',
        '\r','\n',0
    };
    static const CHAR16 error_file[] = {
        'B','o','o','t','l','o','a','d','e','r',':',' ',
        'c','o','u','l','d',' ','n','o','t',' ',
        'r','e','a','d',' ','k','e','r','n','e','l','.','e','l','f',
        '\r','\n',0
    };
    static const CHAR16 error_elf[] = {
        'B','o','o','t','l','o','a','d','e','r',':',' ',
        'i','n','v','a','l','i','d',' ','E','L','F',' ','k','e','r','n','e','l',
        '\r','\n',0
    };
    static const CHAR16 error_memory[] = {
        'B','o','o','t','l','o','a','d','e','r',':',' ',
        'k','e','r','n','e','l',' ','m','e','m','o','r','y',' ',
        'a','l','l','o','c','a','t','i','o','n',' ','f','a','i','l','e','d',
        '\r','\n',0
    };

    EFI_BOOT_SERVICES *bs = (EFI_BOOT_SERVICES *)SystemTable->BootServices;
    EFI_LOCATE_PROTOCOL LocateProtocol = (EFI_LOCATE_PROTOCOL)bs->Functions[37];
    EFI_ALLOCATE_PAGES AllocatePages = (EFI_ALLOCATE_PAGES)bs->Functions[2];

    static const EFI_GUID fs_guid = {
        0x964e5b22, 0x6459, 0x11d2,
        { 0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b }
    };

    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs = 0;
    if (LocateProtocol(&fs_guid, 0, (void **)&fs) != EFI_SUCCESS || fs == 0)
        return fail(SystemTable, error_fs);

    EFI_FILE_PROTOCOL *root = 0;
    if (fs->OpenVolume(fs, &root) != EFI_SUCCESS || root == 0)
        return fail(SystemTable, error_fs);

    static const CHAR16 kernel_path[] = {
        '\\','k','e','r','n','e','l','.','e','l','f',0
    };

    EFI_FILE_PROTOCOL *file = 0;
    if (root->Open(root, &file, kernel_path, 1, 0) != EFI_SUCCESS || file == 0) {
        root->Close(root);
        return fail(SystemTable, error_file);
    }

    void *kernel_buffer = 0;
    if (allocate_pool(SystemTable, KERNEL_FILE_CAPACITY, &kernel_buffer) != EFI_SUCCESS) {
        file->Close(file);
        root->Close(root);
        return fail(SystemTable, error_memory);
    }

    UINTN file_size = KERNEL_FILE_CAPACITY;
    EFI_STATUS read_status = file->Read(file, &file_size, kernel_buffer);
    file->Close(file);
    root->Close(root);

    if (read_status != EFI_SUCCESS ||
        file_size < sizeof(ELF64_HEADER) ||
        file_size >= KERNEL_FILE_CAPACITY) {
        free_pool(SystemTable, kernel_buffer);
        return fail(SystemTable, error_file);
    }

    ELF64_HEADER *elf = (ELF64_HEADER *)kernel_buffer;

    if (elf->ident[0] != 0x7f || elf->ident[1] != 'E' ||
        elf->ident[2] != 'L' || elf->ident[3] != 'F' ||
        elf->ident[4] != 2 || elf->ident[5] != 1 ||
        elf->ident[6] != ELF_VERSION_CURRENT ||
        elf->type != ELF_ET_EXEC || elf->machine != ELF_EM_X86_64 ||
        elf->version != ELF_VERSION_CURRENT ||
        elf->header_size != sizeof(ELF64_HEADER) ||
        elf->program_header_entry_size != sizeof(ELF64_PROGRAM_HEADER) ||
        elf->program_header_count == 0 ||
        elf->program_header_offset > file_size ||
        elf->program_header_count >
            (file_size - elf->program_header_offset) /
                sizeof(ELF64_PROGRAM_HEADER)) {
        free_pool(SystemTable, kernel_buffer);
        return fail(SystemTable, error_elf);
    }

    ELF64_PROGRAM_HEADER *phdrs = (ELF64_PROGRAM_HEADER *)(
        (uint8_t *)kernel_buffer + elf->program_header_offset
    );

    uint64_t load_base = UINT64_MAX;
    uint64_t load_end = 0;
    int entry_is_executable = 0;
    int found_load_segment = 0;

    for (uint16_t i = 0; i < elf->program_header_count; ++i) {
        ELF64_PROGRAM_HEADER *p = &phdrs[i];
        if (p->type != ELF_PT_LOAD) continue;

        if (p->file_size > p->memory_size) {
            free_pool(SystemTable, kernel_buffer);
            return fail(SystemTable, error_elf);
        }
        if (p->memory_size == 0) continue;

        if (p->offset > file_size ||
            p->file_size > file_size - p->offset ||
            p->physical_address != p->virtual_address ||
            p->physical_address < KERNEL_MIN_ADDRESS ||
            p->physical_address >= KERNEL_MAX_ADDRESS ||
            p->memory_size > KERNEL_MAX_ADDRESS - p->physical_address) {
            free_pool(SystemTable, kernel_buffer);
            return fail(SystemTable, error_elf);
        }

        if (p->alignment > 1 &&
            (!is_power_of_two(p->alignment) ||
             (p->offset & (p->alignment - 1)) !=
             (p->virtual_address & (p->alignment - 1)))) {
            free_pool(SystemTable, kernel_buffer);
            return fail(SystemTable, error_elf);
        }

        uint64_t segment_end = p->physical_address + p->memory_size;
        if (p->physical_address < load_base) load_base = p->physical_address;
        if (segment_end > load_end) load_end = segment_end;
        found_load_segment = 1;

        if ((p->flags & ELF_PF_X) != 0 &&
            elf->entry >= p->virtual_address &&
            elf->entry < p->virtual_address + p->memory_size) {
            entry_is_executable = 1;
        }
    }

    if (!found_load_segment || load_base == UINT64_MAX ||
        load_end <= load_base ||
        elf->entry < KERNEL_MIN_ADDRESS ||
        elf->entry >= KERNEL_MAX_ADDRESS ||
        !entry_is_executable) {
        free_pool(SystemTable, kernel_buffer);
        return fail(SystemTable, error_elf);
    }

    load_base &= ~(PAGE_SIZE - 1);
    if (load_end > UINT64_MAX - (PAGE_SIZE - 1)) {
        free_pool(SystemTable, kernel_buffer);
        return fail(SystemTable, error_elf);
    }

    uint64_t allocation_end =
        (load_end + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

    if (allocation_end <= load_base || allocation_end > KERNEL_MAX_ADDRESS) {
        free_pool(SystemTable, kernel_buffer);
        return fail(SystemTable, error_elf);
    }

    UINTN pages = (allocation_end - load_base) / PAGE_SIZE;
    EFI_PHYSICAL_ADDRESS physical_address = load_base;

    if (AllocatePages(EFI_ALLOCATE_ADDRESS, EFI_LOADER_CODE,
                      pages, &physical_address) != EFI_SUCCESS ||
        physical_address != load_base) {
        free_pool(SystemTable, kernel_buffer);
        return fail(SystemTable, error_memory);
    }

    zero_memory((void *)(uintptr_t)load_base, allocation_end - load_base);

    for (uint16_t i = 0; i < elf->program_header_count; ++i) {
        ELF64_PROGRAM_HEADER *p = &phdrs[i];
        if (p->type != ELF_PT_LOAD || p->memory_size == 0 || p->file_size == 0)
            continue;

        copy_memory((void *)(uintptr_t)p->physical_address,
                    (const uint8_t *)kernel_buffer + p->offset,
                    p->file_size);
    }

    *KernelEntryAddress = elf->entry;
    free_pool(SystemTable, kernel_buffer);
    return EFI_SUCCESS;
}

/* =========================================================
   UEFI entry point
   ========================================================= */

EFI_STATUS efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    static const CHAR16 starting[] = {
        'B','o','o','t','l','o','a','d','e','r',':',' ',
        'l','o','a','d','i','n','g',' ','k','e','r','n','e','l','.',
        'e','l','f','\r','\n',0
    };
    static const CHAR16 map_error[] = {
        'B','o','o','t','l','o','a','d','e','r',':',' ',
        'c','o','u','l','d',' ','n','o','t',' ','g','e','t',' ',
        'm','e','m','o','r','y',' ','m','a','p','\r','\n',0
    };

    EFI_BOOT_SERVICES *bs = (EFI_BOOT_SERVICES *)SystemTable->BootServices;
    EFI_GET_MEMORY_MAP GetMemoryMap = (EFI_GET_MEMORY_MAP)bs->Functions[4];
    EFI_EXIT_BOOT_SERVICES ExitBootServices =
        (EFI_EXIT_BOOT_SERVICES)bs->Functions[26];

    SystemTable->ConOut->OutputString(SystemTable->ConOut, starting);

    EFI_PHYSICAL_ADDRESS kernel_entry = 0;
    if (load_kernel(SystemTable, &kernel_entry) != EFI_SUCCESS) {
        for (;;) __asm__ volatile ("cli; hlt");
    }

    BootInfo *boot_info = 0;
    if (allocate_pool(SystemTable, sizeof(BootInfo),
                      (void **)&boot_info) != EFI_SUCCESS) {
        return fail(SystemTable, map_error);
    }

    zero_memory(boot_info, sizeof(BootInfo));

    /*
     * Discover GOP before obtaining the final memory map.
     * Protocol lookup must happen before ExitBootServices.
     */
    discover_framebuffer(SystemTable, boot_info);

    void *memory_map = 0;
    if (allocate_pool(SystemTable, MEMORY_MAP_CAPACITY,
                      &memory_map) != EFI_SUCCESS) {
        free_pool(SystemTable, boot_info);
        return fail(SystemTable, map_error);
    }

    UINTN map_size = MEMORY_MAP_CAPACITY;
    UINTN map_key = 0;
    UINTN descriptor_size = 0;
    uint32_t descriptor_version = 0;
    int exited = 0;

    for (int attempt = 0; attempt < 5; ++attempt) {
        map_size = MEMORY_MAP_CAPACITY;
        descriptor_size = 0;

        EFI_STATUS status = GetMemoryMap(
            &map_size, memory_map, &map_key,
            &descriptor_size, &descriptor_version
        );

        if (status != EFI_SUCCESS || descriptor_size == 0 ||
            map_size > MEMORY_MAP_CAPACITY) {
            return fail(SystemTable, map_error);
        }

        boot_info->memory_map = memory_map;
        boot_info->memory_map_size = map_size;
        boot_info->descriptor_size = descriptor_size;
        boot_info->descriptor_version = descriptor_version;

        status = ExitBootServices(ImageHandle, map_key);
        if (status == EFI_SUCCESS) {
            exited = 1;
            break;
        }
    }

    if (!exited) {
        for (;;) __asm__ volatile ("cli; hlt");
    }

    KERNEL_ENTRY entry = (KERNEL_ENTRY)(uintptr_t)kernel_entry;
    entry(boot_info);

    for (;;) __asm__ volatile ("cli; hlt");
}

#endif
