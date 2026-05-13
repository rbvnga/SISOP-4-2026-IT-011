#define FUSE_USE_VERSION 31

#include <fuse3/fuse.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#define ENC_EXT ".enc"
#define XOR_KEY 0x76
static const char *encrypted_storage = "/app/encrypted_storage";

// Fungsi XOR encrypt/decrypt (sama karena XOR)
static void xor_data(char *buf, size_t size) {
    for (size_t i = 0; i < size; i++) {
        buf[i] ^= XOR_KEY;
    }
}

// Ubah path fuse_mount → encrypted_storage + tambah .enc
static void get_enc_path(char enc_path[], const char *path) {
    snprintf(enc_path, 1024, "%s%s%s", encrypted_storage, path, ENC_EXT);
}

// Cek apakah path adalah direktori
static int is_dir(const char *path) {
    char enc_path[1024];
    snprintf(enc_path, 1024, "%s%s", encrypted_storage, path);
    struct stat st;
    if (stat(enc_path, &st) == 0 && S_ISDIR(st.st_mode)) return 1;
    return 0;
}

static int xmp_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
    char enc_path[1024];
    int res;

    if (is_dir(path)) {
        snprintf(enc_path, 1024, "%s%s", encrypted_storage, path);
        res = stat(enc_path, stbuf);
    } else {
        get_enc_path(enc_path, path);
        res = stat(enc_path, stbuf);
    }

    if (res == -1) return -errno;
    return 0;
}

static int xmp_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                        off_t offset, struct fuse_file_info *fi,
                        enum fuse_readdir_flags flags) {
    char enc_path[1024];
    snprintf(enc_path, 1024, "%s%s", encrypted_storage, path);

    DIR *dp = opendir(enc_path);
    if (!dp) return -errno;

    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);

    struct dirent *de;
    while ((de = readdir(dp)) != NULL) {
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
            continue;

        char name[256];
        strncpy(name, de->d_name, sizeof(name));

        // Hapus .enc dari nama file saat ditampilkan
        char *ext = strstr(name, ENC_EXT);
        if (ext) *ext = '\0';

        filler(buf, name, NULL, 0, 0);
    }

    closedir(dp);
    return 0;
}

static int xmp_mkdir(const char *path, mode_t mode) {
    char enc_path[1024];
    snprintf(enc_path, 1024, "%s%s", encrypted_storage, path);
    if (mkdir(enc_path, mode) == -1) return -errno;
    return 0;
}

static int xmp_rmdir(const char *path) {
    char enc_path[1024];
    snprintf(enc_path, 1024, "%s%s", encrypted_storage, path);
    if (rmdir(enc_path) == -1) return -errno;
    return 0;
}

static int xmp_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    char enc_path[1024];
    get_enc_path(enc_path, path);
    int fd = open(enc_path, fi->flags | O_CREAT, mode);
    if (fd == -1) return -errno;
    fi->fh = fd;
    return 0;
}

static int xmp_open(const char *path, struct fuse_file_info *fi) {
    char enc_path[1024];
    get_enc_path(enc_path, path);
    int fd = open(enc_path, fi->flags);
    if (fd == -1) return -errno;
    fi->fh = fd;
    return 0;
}

static int xmp_read(const char *path, char *buf, size_t size, off_t offset,
                    struct fuse_file_info *fi) {
    int res = pread(fi->fh, buf, size, offset);
    if (res == -1) return -errno;
    xor_data(buf, res); // Dekripsi saat dibaca
    return res;
}

static int xmp_write(const char *path, const char *buf, size_t size, off_t offset,
                     struct fuse_file_info *fi) {
    char *enc_buf = malloc(size);
    if (!enc_buf) return -ENOMEM;
    memcpy(enc_buf, buf, size);
    xor_data(enc_buf, size); // Enkripsi saat ditulis
    int res = pwrite(fi->fh, enc_buf, size, offset);
    free(enc_buf);
    if (res == -1) return -errno;
    return res;
}

static int xmp_truncate(const char *path, off_t size, struct fuse_file_info *fi) {
    char enc_path[1024];
    get_enc_path(enc_path, path);
    if (truncate(enc_path, size) == -1) return -errno;
    return 0;
}

static int xmp_unlink(const char *path) {
    char enc_path[1024];
    get_enc_path(enc_path, path);
    if (unlink(enc_path) == -1) return -errno;
    return 0;
}

static int xmp_access(const char *path, int mask) {
    char enc_path[1024];
    if (is_dir(path))
        snprintf(enc_path, 1024, "%s%s", encrypted_storage, path);
    else
        get_enc_path(enc_path, path);
    if (access(enc_path, mask) == -1) return -errno;
    return 0;
}

static int xmp_utimens(const char *path, const struct timespec ts[2],
                        struct fuse_file_info *fi) {
    char enc_path[1024];
    if (is_dir(path))
        snprintf(enc_path, 1024, "%s%s", encrypted_storage, path);
    else
        get_enc_path(enc_path, path);
    if (utimensat(0, enc_path, ts, 0) == -1) return -errno;
    return 0;
}

static int xmp_release(const char *path, struct fuse_file_info *fi) {
    close(fi->fh);
    return 0;
}
static const struct fuse_operations xmp_oper = {
    .getattr  = xmp_getattr,
    .readdir  = xmp_readdir,
    .mkdir    = xmp_mkdir,
    .rmdir    = xmp_rmdir,
    .create   = xmp_create,
    .open     = xmp_open,
    .read     = xmp_read,
    .write    = xmp_write,
    .truncate = xmp_truncate,
    .unlink   = xmp_unlink,
    .access   = xmp_access,
    .utimens  = xmp_utimens,
    .release  = xmp_release,
};

int main(int argc, char *argv[]) {
    return fuse_main(argc, argv, &xmp_oper, NULL);
}