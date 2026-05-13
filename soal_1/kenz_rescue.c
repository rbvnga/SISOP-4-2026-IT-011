#define FUSE_USE_VERSION 31
#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

static const char *source_dir;// sumber direktori asli: amba_files/
static const char *virtual_file = "tujuan.txt";

// membangun path lengkap ke file di dalam folder sumber (amba_files/)
static void buat_path_lengkap(char *buf, size_t size, const char *path)
{
    snprintf(buf, size, "%s%s", source_dir, path);
}

static char *ambil_koordinat(size_t *output_lenght)
{
    // tempat menyimpan nilai KOORD dari tiap file
    char koordinat[7][256];
    memset(koordinat, 0, sizeof(koordinat));

    // for loop untuk file 1.txt hingga 7.txt
    for (int i = 1; i <= 7; i++) {
        // buat variabel untuk menyimpan alamat file txt nya
        char alamat_file[512];
        snprintf(alamat_file, sizeof(alamat_file), "%s/%d.txt", source_dir, i);

        FILE *f = fopen(alamat_file, "r"); // baca file
        if (!f) continue;

        char line[4096];
        // baca baris per baris
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, "KOORD:", 6) == 0) {
                // ambil nilai setelah "KOORD:"
                char *val = line + 6;
                // skip spasi di depan
                while (*val == ' ' || *val == '\t') val++;
                // simpan nilai
                strncpy(koordinat[i-1], val, sizeof(koordinat[i-1]) - 1);
                // Hapus karakter-karakter tidak perlu di bagian akhir baris
                int end = strlen(koordinat[i-1]) - 1;
                while (end >= 0 && (koordinat[i-1][end] == '\n' ||
                                    koordinat[i-1][end] == '\r' ||
                                    koordinat[i-1][end] == ' '  ||
                                    koordinat[i-1][end] == ',')) {
                    koordinat[i-1][end--] = '\0';
                }
            }
        }
        fclose(f);
    }

    char *content = malloc(4096);
    if (!content) { 
        *output_lenght = 0; return NULL; 
    }

    // Gabungkan semua nilai koordinat 
    int written = snprintf(content, 4096,
        "Tujuan Mas Amba: %s, %s, %s, %s, %s    %s%s\n",
        koordinat[0],
        koordinat[1],
        koordinat[2],
        koordinat[3],
        koordinat[4],
        koordinat[5],
        koordinat[6]
    );

    *output_lenght = (size_t)written;
    return content;
}


  // Dipanggil saat sistem butuh info file (ukuran, tipe, permission)
static int kenz_getattr(const char *path, struct stat *stbuf,
                        struct fuse_file_info *fi)
{
    (void) fi;
    // reset isi stbuf
    memset(stbuf, 0, sizeof(struct stat));

    if (strcmp(path, "/") == 0) {
        stbuf->st_mode  = S_IFDIR | 0755;
        //Set jumlah hard link ke 2
        stbuf->st_nlink = 2;
        return 0;
    }
    // Cek apakah file yang ditanya adalah tujuan.txt
    if (strcmp(path + 1, virtual_file) == 0) {
        size_t len;
        // Panggil fungsi "ambil_kooerdinat" untuk mendapatkan ukurannya (len).
        char *content = ambil_koordinat(&len);
        // buang isi content
        free(content);

        stbuf->st_mode  = S_IFREG | 0444;
        // Set jumlah hard link ke 1
        stbuf->st_nlink = 1;
        // Set ukuran file 
        stbuf->st_size  = (off_t)len;
        return 0;
    }

    char src_path[512];
    buat_path_lengkap(src_path, sizeof(src_path), path);
    // Ambil info file langsung dari disk 
    int res = lstat(src_path, stbuf);
    // Kalau lstat gagal 
    if (res == -1) return -errno;
    return 0;
}

static int kenz_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                        off_t offset, struct fuse_file_info *fi,
                        enum fuse_readdir_flags flags)
{
    //unused variable
    (void) offset;
    (void) fi;
    (void) flags;

    //cek apakah yang akan diperiksa root directory
    if (strcmp(path, "/") != 0)
        return -ENOENT;

    // penambahan entry untuk fokder mnt
    filler(buf, ".",  NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);
    // Tambahkan tujuan.txt ke daftar
    filler(buf, virtual_file, NULL, 0, 0);


    // buka directory amba
    DIR *dp = opendir(source_dir);
    if (!dp) return -errno;

    struct dirent *de; // variabel untuk menampung info tiap entry (file/folder) yang dibaca dari direktori
    // baca isi folder sampai habis
    while ((de = readdir(dp)) != NULL) {
        // Lewati entry tertentu untuk amba_files
        if (de->d_name[0] == '.') continue;
        // tambahkan nama file dari forlder amba
        filler(buf, de->d_name, NULL, 0, 0);
    }
    closedir(dp);
    return 0;
}

static int kenz_open(const char *path, struct fuse_file_info *fi)
{
    if (strcmp(path + 1, virtual_file) == 0){
    // jika ingin membuka file virtual
        return 0;
    }

    // dapatkan alamat
    char src_path[512];
    buat_path_lengkap(src_path, sizeof(src_path), path);
    // buka file
    int fd = open(src_path, fi->flags);
    if (fd == -1) return -errno;

    close(fd);
    return 0;
}

static int kenz_read(const char *path, char *buf, size_t size,
                     off_t offset, struct fuse_file_info *fi)
{
    (void) fi; // unused
    // jika akan membaca file virtual 
    if (strcmp(path + 1, virtual_file) == 0) {
        size_t len;
        char *content = ambil_koordinat(&len);
        // Cek apakah posisi baca (offset) sudah melewati akhir konten
        if ((size_t)offset >= len) {
            free(content);
            return 0;
        }
        // hitung berapa bytes yang tersisa untuk di baca
        size_t bytes = len - (size_t)offset;
        if (bytes > size) bytes = size;
        // salin isi konten sebanyak bytes ke buf
        memcpy(buf, content + offset, bytes); // pembacaan dari posisi offset
        free(content);
        return (int)bytes;
    }
    // jika file yang dibaca adalah passthrough
    char src_path[512];
    buat_path_lengkap(src_path, sizeof(src_path), path);
    // buka dan baca file
    int fd = open(src_path, O_RDONLY);
    if (fd == -1) return -errno;
    int res = pread(fd, buf, size, offset);
    if (res == -1) res = -errno;

    close(fd);
    return res;
}

// daftar fungsi callback yang akan digunakan FUSE 
static struct fuse_operations kenz_ops = {
    .getattr = kenz_getattr,
    .readdir = kenz_readdir,
    .open    = kenz_open,
    .read    = kenz_read,
};


int main(int argc, char *argv[])
{
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <source_directory> <mount_directory>\n", argv[0]);
        return 1;
    }

    source_dir = realpath(argv[1], NULL);
    if (!source_dir) {
        perror("realpath source_dir");
        return 1;
    }
    // geser argv 
    argv[1] = argv[2];
    argc--;
    return fuse_main(argc, argv, &kenz_ops, NULL);
}