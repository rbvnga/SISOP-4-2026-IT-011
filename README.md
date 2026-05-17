# SISOP-4-2026-IT-011
Nama : Revalinda Bunga Nayla Laksono <br>
NRP : 5027251011 <br>
# Soal 1
## Deskripsi Permasalahan
Menyelamatkan asisten Kenz dengan mengimplementasikan sebuah **filesystem virtual** menggunakan teknologi FUSE _(Filesystem in Userspace)_. Program yang dibuat harus mampu menampilkan isi dari direktori sumber ke dalam sebuah direktori mount secara transparan, seolah-olah kedua direktori tersebut memiliki isi yang sama, serta menambahkan sebuah file virtual yang tidak tersimpan secara fisik di disk namun tetap dapat diakses layaknya file biasa melalui direktori mount.
### Mengambil arsip `amba_files`
```c
gdown 1nLXFhptDo2mnUlZsw8pTWyAVpV49W20U 
unzip amba_files.zip
rm amba_files.zip
```
folder zip amba diambil lalu diekstrak. Setelah folder berhasil di ekstrak, folder zip akan dihapus. <br>
### Menemukan kooerdinat ritual
```c
static char *ambil_koordinat(size_t *out_len)
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
        *out_len = 0; return NULL; 
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
```
Baca setiap file txt menggunakan perulangan `for` loop, lalu baca setiap baris hingga menemukan `KOORD:` lalu ambil string setelahnya untuk mengambil niali koordinat dengan mengabaikan karaktaer=karakter yang tidak diperlukan. Minta memori di heap untuk menyimpan hasil tersebut, lalu gabungkan semuanya. `written` akan berisi jumlah karakter yang berhasil ditulis. Simpan panjang string ke `output_lenght` supaya pemanggil fungsi ini tahu ukurannya (dibutuhkan oleh `getattr` untuk mengisi `st_size`). <br>
<img width="608" height="264" alt="1_Koordinat" src="https://github.com/user-attachments/assets/a3125068-a851-4b33-83ea-2db1dd0968eb" /> <br>

```c
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
```
`kenz_getattr` dipanggil otomatis oleh FUSE setiap kali sistem butuh informasi tentang sebuah file atau folder. <br>
- `path` = lokasi file yang di cari tahu, misalnya /1.txt
- `stbuf` = tempat menyimpan hasil info file
- tipe direktori `S_IFDIR` memberikan permission `rwxr-xr-x`
- tipe direktori `S_IFREG` memberikan permission `r--r--r-- ` <br>
ukuran file (`len`) diperoleh dengan memanggil function `ambil_koordinat()` lalu isi contentnya langsung dibuang dengan `free()` karena hanya butuh ukurannya saja. Set ukuran file sesuai panjang konten yang akan dikembalikan oleh `read()`, `stat` dan `read` konsisten  <br>
### Menampilkan list file dari folder 
```c
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
```
fungsi mengecek apakah yang di-ls benar-benar root directory /. Setelah itu, fungsi menambahkan tiga entry pertama secara manual ke dalam daftar, yaitu ., .., dan tujuan.txt (karena tidak benar-benar ada di disk).  Setelah itu, fungsi membuka folder amba_files/ dan membaca isinya satu per satu, lalu menambahkan setiap nama file yang ditemukan ke dalam daftar, dengan pengecualian entry . dan .. yang dilewati karena sudah ditambahkan sebelumnya. Setelah semua file terdaftar, folder ditutup dan fungsi selesai, sehingga hasil akhir ls mnt/ menampilkan delapan entry yaitu 1.txt sampai 7.txt ditambah tujuan.txt. <br>
<img width="549" height="147" alt="1_File Virtual" src="https://github.com/user-attachments/assets/677b4ce7-d301-409d-a11a-e53a472182b4" /> <br>

### Menampilkan Output Identik
```c
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
```
`kenz_open`  dipanggil otomatis oleh FUSE setiap kali ada file yang akan dibuka. File sengaja dibuka dan langsung ditutup di sini karena fungsi open di FUSE hanya bertugas memvalidasi apakah file bisa dibuka, bukan untuk membaca isinya. Pembacaan isi dilakukan nanti oleh `kenz_read()`. <br>
- `path` = lokasi file yang mau dibuka
- `fd` = nomor ID yang dipakai sistem untuk merujuk file yang sedang dibuka. (file descriptor)
- `buf` = tempat menampung hasil bacaan 
- `size` = berapa byte yang ingin dibaca
- `offset` = mulai baca dari posisi ke berapa <br>
`kenz_read()` mengisi buffer (`buf`) yang diberikan kernel, lalu kernel yang meneruskan isi buffer tersebut ke program pemanggil seperti cat untuk ditampilkan ke layar.
Saat file yang dibaca adalah tujuan.txt, fungsi memanggil ambil_koordinat() untuk membuat isi file secara on-the-fly di memori. Sebelum menyalin data ke buffer, fungsi mengecek apakah posisi baca (offset) sudah melewati akhir konten — kalau sudah, langsung return 0 sebagai tanda tidak ada lagi yang perlu dibaca. Kalau belum, fungsi menghitung berapa byte yang perlu disalin, lalu membatasinya agar tidak melebihi kapasitas buffer supaya tidak terjadi overflow. <br>
Saat file yang dibaca adalah file passthrough, fungsi membangun path lengkap ke file asli di amba_files/, lalu membukanya dengan mode read-only. `pread` digunakan agar bisa membaca dari posisi tertentu tanpa menggeser posisi baca file, sehingga aman dipanggil berkali-kali dari posisi berbeda. Setelah selesai, file ditutup dan jumlah byte yang berhasil dibaca dikembalikan ke kernel. <br>
<img width="924" height="432" alt="1_output identik" src="https://github.com/user-attachments/assets/73201c5d-d38f-415f-b908-0c18d3ce0a91" /> <br>
```c
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
```
<img width="622" height="550" alt="1_mnt" src="https://github.com/user-attachments/assets/47361482-d376-4d4f-a403-3a209f78e735" /> <br>

# Soal 2
## Deskripsi Permasalahan
Membuat mini database service yang dapat diakses melalui TCP Connection bernama project MOO. <br>
## fuse.c 
```c
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
// Ekstensi file terenkripsi
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
```
fuse.c akan menghubungkan dua direktori `encrypted_storage` (file terenkripsi) dan direktori `fuse_mount` (file terbaca normal). Setiap file yang ditulis melalui fuse_mount akan otomatis dienkripsi menggunakan algoritma **XOR dengan key 0x76 **dan disimpan di `encrypted_storage` dengan ekstensi `.enc`. Sebaliknya, saat dibaca melalui `fuse_mount`, file akan otomatis didekripsi. <br>
- `XOR_data()` , melakukan enkripsi dan Dekripsi dengen operasi **XOR** yang bersifat simetris, pada setiap byte data dengan key `0x76`.
- `get_enc_path()` Mengubah path virtual (dari `fuse_mount`) menjadi path nyata di `encrypted_storage` dan menambahkan ekstensi `.enc` di akhir nama file
- `is_dir()` akan mengecek apakah sebuah path adalah direktori atau file, karena direktori tidak memerlukan ekstensi `.enc`
- `fuse_operations` adalah struct yang mendaftarkan semua fungsi ke FUSE
### Implementasi Operasi FUSE
1. `xmp_getattr()` Dipanggil setiap kali sistem perlu mengetahui informasi file (ukuran, permission, waktu modifikasi) <br>
- Jika path adalah direktori → ambil atribut dari `encrypted_storage/path`
- Jika path adalah file → ambil atribut dari `encrypted_storage/path.enc` <br>
2. `xmp_readdir()` Dipanggil saat perintah `ls` dijalankan di dalam fuse_mount <br>
3. `xmp_mkdir()` untuk membuat folder baru di `encrypted_storage` <br>
4. `xmp_rmdir()` untuk menghapus folder dari `encrypted_storage` <br>
5. `xmp_create()` Dipanggil saat file baru dibuat di `fuse_mount` <br>
6. `xmp_open()` Dipanggil saat file dibuka (untuk dibaca/ditulis) <br>
7. `xmp_read()` untuk membaca file <br>
- program akan membaca data dari file `.enc` menggunakan `pread()`
- Mendekripsi data dengan `xor_data()` sebelum dikembalikan ke user <br>
8. `xmp_write()` untuk menulis isi file dan mengenkripsinya <br>
- program akan menerima dan mengenkrisi data dari user dengan `xor_data()`
- Menyimpan data terenkripsi ke file `.enc` menggunakan `pwrite()`
- Menggunakan `malloc()` untuk buffer sementara agar data asli tidak berubah <br>
9. `xmp_truncate()` untuk mengubah ukuran file di `encrypted_storage`, fungsi ini dipaggil saat file di-overwrite atau dikosongkan <br>
10. `xmp_unlink()` untuk menghapus file `.enc` dari `encrypted_storage`, fungsi ini dipanggil saat perintah `rm` di jalankan di fuse_mount <br>
11. `xmp_access()` untuk mengecek apakah user punya izin untuk mengakses file/folder <br>
12. `xmp_utimens()` untuk memperbarui timestamp (waktu akses dan modifikasi) file, fungsi ini dipanggil otomatis saat file dibuat atau dimodifikasi. <br>
13. `xmp_release()` akan menutup file descriptor setelah selesai digunakan supaya mencegah memory/resource leak <br>
## client.c 
```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>

#define PORT 9000
#define HOST "127.0.0.1"
#define BUFFER_SIZE 4096

int main() {
    // inisialisasi Socket 
    int sock;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];
    char command[BUFFER_SIZE];

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Socket failed");
        exit(1);
    }

   // Konfigurasi alamat server
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, HOST, &server_addr.sin_addr);

    // Koneksi ke Server
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        exit(1);
    }
    printf("Connected to DB Server on port %d\n", PORT);
    printf("Type HELP for available commands\n");
    printf("Type EXIT to quit\n\n");

    while (1) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(sock, &fds);
        FD_SET(STDIN_FILENO, &fds);

        select(sock + 1, &fds, NULL, NULL, NULL);

        // Ada data dari server
        if (FD_ISSET(sock, &fds)) {
            memset(buffer, 0, BUFFER_SIZE);
            int bytes = recv(sock, buffer, BUFFER_SIZE - 1, 0);
            if (bytes <= 0) {
                printf("Server disconnected\n");
                break;
            }
            printf("%s", buffer);
            fflush(stdout);
        }

        // Mengirim perintah ke server (ada input dari user)
        if (FD_ISSET(STDIN_FILENO, &fds)) {
            memset(command, 0, BUFFER_SIZE);
            printf("db > ");
            fflush(stdout);
            if (fgets(command, BUFFER_SIZE, stdin) == NULL) break;
            send(sock, command, strlen(command), 0);
            if (strncmp(command, "EXIT", 4) == 0) break;
        }
    }

   // penutupan koneksi
    close(sock);
    return 0;
}
```
Pada loop utama, `select()` digunakan supaya program tidak harus memilih salah satu dari: menunggu input user atau menunggu response server. Dengan `select()`, , program bisa memantau dua sumber input secara bersamaan, yaitu: <br>
- `sock` yang bersumber data yang masuk dari server
- `STDIN_FILENO` yang menunggu input dari keyboard user <br>
`select()` akan memblokir (menunggu) sampai salah satu dari keduanya ada data, lalu melanjutkan eksekusi. <br>
## Dockerfile
```
FROM ubuntu:latest

RUN apt-get update && apt-get install -y \
    fuse3 \
    libfuse3-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY server fuse entrypoint.sh .
RUN chmod +x server fuse entrypoint.sh
RUN mkdir -p /app/db /app/encrypted_storage

EXPOSE 9000
CMD ["./entrypoint.sh"]
```
Dockerfile digunakan untuk membangun image Docker dari aplikasi mini database MOO. diawali dengan penentuan base image lalu instalansi dpendencies kemudian  penetapan direktori kerja `/app` menggunakan `WORKDIR`. Terdapat 3 file utama yaitu server, fuse, dan entrypoint.sh disalin ke dalam container menggunakan COPY dan diberikan permission execute menggunakan chmod +x, serta dibuat dua direktori `/app/db` sebagai mount point FUSE dan `/app/encrypted_storage` sebagai tempat penyimpanan file terenkripsi. <br>
## entrypoint.sh
```````bash
mkdir -p /app/db
./fuse /app/db    # Mount FUSE dulu
sleep 1           # Tunggu FUSE siap
exec ./server     # Baru jalankan server
```````
<img width="703" height="137" alt="2_containerization" src="https://github.com/user-attachments/assets/84976813-99c1-4676-a414-32b9ac6fd2fc" /> <br>
<img width="1729" height="749" alt="2_hasil eksekusi" src="https://github.com/user-attachments/assets/9f1799a4-0797-4358-a3dc-dce53ec1d50e" /> <br>
<img width="1141" height="79" alt="2_Docker" src="https://github.com/user-attachments/assets/239b712b-d1e5-4563-82cc-f6ccbd67ae08" /> <br>



# Soal 3
## Deskripsi Permasalahan
IT Library Nusantara adalah perpustakaan digital khusus bidang teknologi Informasi. Sebagai System Administrator baru di IT Library Nusantara, harus membangun infrastruktur Library IT dari nol menggunaknan DOcker dan Samba.  <br>
