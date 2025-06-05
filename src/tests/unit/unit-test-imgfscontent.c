#include "image_content.h"
#include "imgfs.h"
#include "test.h"
#include "imgfscmd_functions.h"

#include <check.h>
#include <vips/vips.h>

#if VIPS_MINOR_VERSION >= 15
// these are the values we got in 8.15.1
#define SMALL_RES_SIZE_A  16296
#define FILE_SIZE_A      208955
#else
// these are the values we got in 8.12.1
#define SMALL_RES_SIZE_A  16299
#define FILE_SIZE_A      208958
#endif

// ======================================================================
START_TEST(lazily_resize_null_params)
{
    start_test_print;

    ck_assert_invalid_arg(lazily_resize(ORIG_RES, NULL, 0));

    end_test_print;
}
END_TEST

// ======================================================================
START_TEST(lazily_resize_out_of_range_index)
{
    start_test_print;
    DECLARE_DUMP;
    DUPLICATE_FILE(dump, IMGFS("test02"));

    struct imgfs_file file;
    ck_assert_err_none(do_open(dump, "rb+", &file));

    ck_assert_err(lazily_resize(ORIG_RES, &file, 218), ERR_INVALID_IMGID);

    do_close(&file);

    end_test_print;
}
END_TEST

// ======================================================================
START_TEST(lazily_resize_empty_index)
{
    start_test_print;
    DECLARE_DUMP;
    DUPLICATE_FILE(dump, IMGFS("test02"));

    struct imgfs_file file;

    ck_assert_err_none(do_open(dump, "rb+", &file));

    ck_assert_err(lazily_resize(ORIG_RES, &file, 3), ERR_INVALID_IMGID);

    do_close(&file);

    end_test_print;
}
END_TEST

// ======================================================================
START_TEST(lazily_resize_res_orig)
{
    start_test_print;
    DECLARE_DUMP;
    DUPLICATE_FILE(dump, IMGFS("test02"));

    long file_size;
    struct imgfs_file file;

    ck_assert_err_none(do_open(dump, "rb+", &file));

    ck_assert_err_none(lazily_resize(ORIG_RES, &file, 0));

    ck_assert_int_eq(fseek(file.file, 0, SEEK_END), 0);
    file_size = ftell(file.file);

    ck_assert_int_eq(file_size, 192659);

    do_close(&file);

    end_test_print;
}
END_TEST

// ======================================================================
START_TEST(lazily_resize_invalid_mode)
{
    start_test_print;
    DECLARE_DUMP;
    DUPLICATE_FILE(dump, IMGFS("test02"));

    struct imgfs_file file;
    ck_assert_err_none(do_open(dump, "rb", &file));

    ck_assert_err(lazily_resize(THUMB_RES, &file, 0), ERR_IO);

    do_close(&file);

    end_test_print;
}
END_TEST

// ======================================================================
START_TEST(lazily_resize_already_exists)
{
    start_test_print;
    DECLARE_DUMP;
    DUPLICATE_FILE(dump, IMGFS("test03"));
    long file_size;
    struct imgfs_file file;

    ck_assert_err_none(do_open(dump, "rb+", &file));

    ck_assert_err_none(lazily_resize(THUMB_RES, &file, 1));

    ck_assert_int_eq(fseek(file.file, 0, SEEK_END), 0);
    file_size = ftell(file.file);

    ck_assert_uint_eq(file_size, 250754);
    ck_assert_uint_eq(file.metadata[1].offset[THUMB_RES], 221108);
    ck_assert_uint_eq(file.metadata[1].size[THUMB_RES], 12319);

    do_close(&file);

    end_test_print;
}
END_TEST

// ======================================================================
START_TEST(lazily_resize_valid)
{
    start_test_print;
    DECLARE_DUMP;
    DUPLICATE_FILE(dump, IMGFS("test02"));

    FILE *reference;
    char reference_buffer[SMALL_RES_SIZE_A], buffer[SMALL_RES_SIZE_A];
    long file_size;
    struct imgfs_file file;

#define FILENAME DATA_DIR "papillon256_256-" VIPS_VERSION ".jpg"
    reference = fopen(FILENAME, "rb");
    ck_assert_msg(reference != NULL, "cannot open file \"" FILENAME "\"\n"
                  "Please check your VIPS version with:\n  vips --version\n"
                  "(on the command line) and maybe let us know (not guaranted.\n"
                  "  Currently supported versions are:\n   - 8.12.1 (on Ubuntu 22.04; incl. EPFL VMs); and\n"
                  "   - 8.15.1 (latest stable version when the project was developped).\n)");

    ck_assert_int_eq(fread(reference_buffer, 1, SMALL_RES_SIZE_A, reference), SMALL_RES_SIZE_A);

    ck_assert_err_none(do_open(dump, "rb+", &file));

    ck_assert_err_none(lazily_resize(SMALL_RES, &file, 0));

    ck_assert_int_eq(fseek(file.file, 0, SEEK_END), 0);
    file_size = ftell(file.file);

    ck_assert_uint_eq(file_size, FILE_SIZE_A);

    ck_assert_uint_eq(file.metadata[0].offset[SMALL_RES], 192659);
    ck_assert_uint_eq(file.metadata[0].size[SMALL_RES]  , SMALL_RES_SIZE_A);

    ck_assert_int_eq(fseek(file.file, 192659, SEEK_SET), 0);
    ck_assert_int_eq(fread(buffer, 1, SMALL_RES_SIZE_A, file.file), SMALL_RES_SIZE_A);
    ck_assert_mem_eq(reference_buffer, buffer, SMALL_RES_SIZE_A);

    fclose(reference);
    do_close(&file);

    // Checks that metadata is correctly persisted
    ck_assert_err_none(do_open(dump, "rb+", &file));

    ck_assert_int_eq(fseek(file.file, 0, SEEK_END), 0);
    file_size = ftell(file.file);

    ck_assert_uint_eq(file_size, FILE_SIZE_A);
    ck_assert_uint_eq(file.metadata[0].offset[SMALL_RES], 192659);
    ck_assert_uint_eq(file.metadata[0].size[SMALL_RES], SMALL_RES_SIZE_A);

    do_close(&file);

    end_test_print;
}
END_TEST


//
START_TEST(lazily_resize_valid_coquelicot_small)
{
    start_test_print;

    VIPS_INIT("");

    char* filename = "../data/coquelicots.jpg";

    FILE* coq_file = fopen(filename, "rb");

    struct imgfs_file database;
    const char* imgfs_filename = "test_01.imgfs";
    const char* max_files_flag = "-max_files";
    const char* max_files = "3";
    const char* thumb_res_flag = "-thumb_res";
    const char* thumb_res_width = "64";
    const char* thumb_res_height = "42";
    const char* small_res_flag = "-small_res";
    const char* small_res_width = "256";
    const char* small_res_height = "170";
    
    int argc = 9;
    char* argv_old[9] = {imgfs_filename, max_files_flag, max_files, thumb_res_flag, thumb_res_width, thumb_res_height, small_res_flag, small_res_width, small_res_height};
    char** argv = (char**) argv_old;

    ck_assert_err_none(do_create_cmd(argc, argv));
    ck_assert_err_none(do_open(imgfs_filename, "rb+", &database));

    long offset = ftell(database.file);

    VipsImage* in = NULL;
    fseek(coq_file, 0, SEEK_END);
    long size = ftell(coq_file);
    fseek(coq_file, 0, SEEK_SET);
    void* in_buffer = calloc(1, size);
    fread(in_buffer, size, 1, coq_file);
    vips_jpegload_buffer(in_buffer, size, &in, NULL);

    size_t out_len = 0;
    void* buffer = NULL;
    vips_jpegsave_buffer(in, &buffer, &out_len, NULL);
    
    fseek(database.file, sizeof(struct imgfs_header) + 3 * sizeof(struct img_metadata), SEEK_SET);
    fwrite(buffer, out_len, 1UL, database.file);

    database.header.nb_files++;
    database.header.version++;

    fseek(database.file, 0, SEEK_SET);

    fwrite(&database.header, sizeof(struct imgfs_header), 1UL, database.file);
    
    database.metadata[0].img_id[0] = 'c';
    database.metadata[0].img_id[1] = '\0';
    database.metadata[0].SHA[0] = '1';
    database.metadata[0].SHA[1] = '\0';
    
    database.metadata[0].is_valid = 1;
    database.metadata[0].offset[ORIG_RES] = offset;
    database.metadata[0].size[ORIG_RES] = out_len;
    unsigned int orig_res_new[ORIG_RES] = {1200, 800};
    memcpy(database.metadata[0].orig_res, orig_res_new, 2 * sizeof(unsigned int));

    fseek(database.file, sizeof(struct imgfs_header), SEEK_SET);
    fwrite(database.metadata, sizeof(struct img_metadata), 1UL, database.file);

    VipsImage* in_small = NULL;
    char* small_filename = "../data/coquelicots_small.jpg";
    FILE* coq_file_small = fopen(small_filename, "rb");
    fseek(coq_file_small, 0, SEEK_END);
    long small_size = ftell(coq_file_small);
    fseek(coq_file_small, 0, SEEK_SET);
    void* in_buffer_small = calloc(1, small_size);
    fread(in_buffer_small, small_size, 1, coq_file_small);
    vips_jpegload_buffer(in_buffer_small, small_size, &in_small, NULL);

    size_t out_len_small = 0;
    void* small_image_buffer_reference = NULL;
    vips_jpegsave_buffer(in_small, &small_image_buffer_reference, &out_len_small);

    ck_assert_err_none(lazily_resize(SMALL_RES, &database, 0));
    fseek(database.file, database.metadata[0].offset[SMALL_RES], SEEK_SET);

    void* small_image_buffer = calloc(1, out_len_small);
    fread(small_image_buffer, out_len_small, 1, database.file);

    ck_assert_mem_eq(small_image_buffer_reference, small_image_buffer, out_len_small);
    
    g_object_unref(VIPS_OBJECT(in));
    g_object_unref(VIPS_OBJECT(in_small));

    free(buffer);
    free(small_image_buffer);
    free(in_buffer);
    free(in_buffer_small);
    free(small_image_buffer_reference);

    fclose(coq_file);
    fclose(coq_file_small);

    do_close(&database);

    end_test_print;
}
END_TEST


START_TEST(lazily_resize_valid_coquelicots_thumbnail)
{
    start_test_print;

    VIPS_INIT("");

    char* filename = "../data/coquelicots.jpg";

    FILE* coq_file = fopen(filename, "rb");

    struct imgfs_file database;
    const char* imgfs_filename = "test_01.imgfs";
    const char* max_files_flag = "-max_files";
    const char* max_files = "3";
    const char* thumb_res_flag = "-thumb_res";
    const char* thumb_res_width = "64";
    const char* thumb_res_height = "42";
    const char* small_res_flag = "-small_res";
    const char* small_res_width = "256";
    const char* small_res_height = "170";
    
    int argc = 9;
    char* argv_old[9] = {imgfs_filename, max_files_flag, max_files, thumb_res_flag, thumb_res_width, thumb_res_height, small_res_flag, small_res_width, small_res_height};
    char** argv = (char**) argv_old;

    ck_assert_err_none(do_create_cmd(argc, argv));
    ck_assert_err_none(do_open(imgfs_filename, "rb+", &database));

    long offset = ftell(database.file);

    VipsImage* in = NULL;
    fseek(coq_file, 0, SEEK_END);
    long size = ftell(coq_file);
    fseek(coq_file, 0, SEEK_SET);
    void* in_buffer = calloc(1, size);
    fread(in_buffer, size, 1, coq_file);
    vips_jpegload_buffer(in_buffer, size, &in, NULL);

    size_t out_len = 0;
    void* buffer = NULL;
    vips_jpegsave_buffer(in, &buffer, &out_len, NULL);
    
    fseek(database.file, sizeof(struct imgfs_header) + 3 * sizeof(struct img_metadata), SEEK_SET);
    fwrite(buffer, out_len, 1UL, database.file);

    database.header.nb_files++;
    database.header.version++;

    fseek(database.file, 0, SEEK_SET);

    fwrite(&database.header, sizeof(struct imgfs_header), 1UL, database.file);
    
    database.metadata[0].img_id[0] = 'd';
    database.metadata[0].img_id[1] = '\0';
    database.metadata[0].SHA[0] = '2';
    database.metadata[0].SHA[1] = '\0';
    
    database.metadata[0].is_valid = 1;
    database.metadata[0].offset[ORIG_RES] = offset;
    database.metadata[0].size[ORIG_RES] = out_len;
    unsigned int orig_res_new[ORIG_RES] = {1200, 800};
    memcpy(database.metadata[0].orig_res, orig_res_new, 2 * sizeof(unsigned int));

    fseek(database.file, sizeof(struct imgfs_header), SEEK_SET);
    fwrite(database.metadata, sizeof(struct img_metadata), 1UL, database.file);

    VipsImage* in_small = NULL;
    char* small_filename = "../data/coquelicots_thumb.jpg";
    FILE* coq_file_small = fopen(small_filename, "rb");
    fseek(coq_file_small, 0, SEEK_END);
    long small_size = ftell(coq_file_small);
    fseek(coq_file_small, 0, SEEK_SET);
    void* in_buffer_small = calloc(1, small_size);
    fread(in_buffer_small, small_size, 1, coq_file_small);
    vips_jpegload_buffer(in_buffer_small, small_size, &in_small, NULL);

    size_t out_len_small = 0;
    void* small_image_buffer_reference = NULL;
    vips_jpegsave_buffer(in_small, &small_image_buffer_reference, &out_len_small);

    ck_assert_err_none(lazily_resize(THUMB_RES, &database, 0));
    fseek(database.file, database.metadata[0].offset[THUMB_RES], SEEK_SET);

    void* small_image_buffer = calloc(1, out_len_small);
    fread(small_image_buffer, out_len_small, 1, database.file);

    ck_assert_int_eq(memcmp(small_image_buffer_reference, small_image_buffer, out_len_small), 0);

    g_object_unref(VIPS_OBJECT(in));
    g_object_unref(VIPS_OBJECT(in_small));

    free(buffer);
    free(small_image_buffer);
    free(in_buffer);
    free(in_buffer_small);
    free(small_image_buffer_reference);

    fclose(coq_file);
    fclose(coq_file_small);

    do_close(&database);

    end_test_print;
}
END_TEST

START_TEST(lazily_resize_valid_papillon_thumbnail)
{
    start_test_print;

    VIPS_INIT("");

    char* filename = "../data/papillon.jpg";

    FILE* coq_file = fopen(filename, "rb");

    struct imgfs_file database;
    const char* imgfs_filename = "test_01.imgfs";
    const char* max_files_flag = "-max_files";
    const char* max_files = "3";
    const char* thumb_res_flag = "-thumb_res";
    const char* thumb_res_width = "64";
    const char* thumb_res_height = "43";
    const char* small_res_flag = "-small_res";
    const char* small_res_width = "256";
    const char* small_res_height = "171";
    
    int argc = 9;
    char* argv_old[9] = {imgfs_filename, max_files_flag, max_files, thumb_res_flag, thumb_res_width, thumb_res_height, small_res_flag, small_res_width, small_res_height};
    char** argv = (char**) argv_old;

    ck_assert_err_none(do_create_cmd(argc, argv));
    ck_assert_err_none(do_open(imgfs_filename, "rb+", &database));

    long offset = ftell(database.file);

    VipsImage* in = NULL;
    fseek(coq_file, 0, SEEK_END);
    long size = ftell(coq_file);
    fseek(coq_file, 0, SEEK_SET);
    void* in_buffer = calloc(1, size);
    fread(in_buffer, size, 1, coq_file);
    vips_jpegload_buffer(in_buffer, size, &in, NULL);

    size_t out_len = 0;
    void* buffer = NULL;
    vips_jpegsave_buffer(in, &buffer, &out_len, NULL);
    
    fseek(database.file, sizeof(struct imgfs_header) + 3 * sizeof(struct img_metadata), SEEK_SET);
    fwrite(buffer, out_len, 1UL, database.file);

    database.header.nb_files++;
    database.header.version++;

    fseek(database.file, 0, SEEK_SET);

    fwrite(&database.header, sizeof(struct imgfs_header), 1UL, database.file);
    
    database.metadata[0].img_id[0] = 'd';
    database.metadata[0].img_id[1] = '\0';
    database.metadata[0].SHA[0] = '2';
    database.metadata[0].SHA[1] = '\0';
    
    database.metadata[0].is_valid = 1;
    database.metadata[0].offset[ORIG_RES] = offset;
    database.metadata[0].size[ORIG_RES] = out_len;
    unsigned int orig_res_new[ORIG_RES] = {1200, 800};
    memcpy(database.metadata[0].orig_res, orig_res_new, 2 * sizeof(unsigned int));

    fseek(database.file, sizeof(struct imgfs_header), SEEK_SET);
    fwrite(database.metadata, sizeof(struct img_metadata), 1UL, database.file);

    VipsImage* in_small = NULL;
    char* small_filename = "../data/papillon_thumb.jpg";
    FILE* coq_file_small = fopen(small_filename, "rb");
    fseek(coq_file_small, 0, SEEK_END);
    long small_size = ftell(coq_file_small);
    fseek(coq_file_small, 0, SEEK_SET);
    void* in_buffer_small = calloc(1, small_size);
    fread(in_buffer_small, small_size, 1, coq_file_small);
    vips_jpegload_buffer(in_buffer_small, small_size, &in_small, NULL);

    size_t out_len_small = 0;
    void* small_image_buffer_reference = NULL;
    vips_jpegsave_buffer(in_small, &small_image_buffer_reference, &out_len_small);

    ck_assert_err_none(lazily_resize(THUMB_RES, &database, 0));
    fseek(database.file, database.metadata[0].offset[THUMB_RES], SEEK_SET);

    void* small_image_buffer = calloc(1, out_len_small);
    fread(small_image_buffer, out_len_small, 1, database.file);

    ck_assert_int_eq(memcmp(small_image_buffer_reference, small_image_buffer, out_len_small), 0);

    g_object_unref(VIPS_OBJECT(in));
    g_object_unref(VIPS_OBJECT(in_small));

    free(buffer);
    free(small_image_buffer);
    free(in_buffer);
    free(in_buffer_small);
    free(small_image_buffer_reference);

    fclose(coq_file);
    fclose(coq_file_small);

    do_close(&database);

    end_test_print;
}
END_TEST

START_TEST(lazily_resize_valid_pic1_small)
{
    start_test_print;

    VIPS_INIT("");

    char* filename = "../data/pic1_orig.jpg";

    FILE* coq_file = fopen(filename, "rb");

    struct imgfs_file database;
    const char* imgfs_filename = "test_01.imgfs";
    const char* max_files_flag = "-max_files";
    const char* max_files = "3";
    const char* thumb_res_flag = "-thumb_res";
    const char* thumb_res_width = "64";
    const char* thumb_res_height = "43";
    const char* small_res_flag = "-small_res";
    const char* small_res_width = "256";
    const char* small_res_height = "256";
    
    int argc = 9;
    char* argv_old[9] = {imgfs_filename, max_files_flag, max_files, thumb_res_flag, thumb_res_width, thumb_res_height, small_res_flag, small_res_width, small_res_height};
    char** argv = (char**) argv_old;

    ck_assert_err_none(do_create_cmd(argc, argv));
    ck_assert_err_none(do_open(imgfs_filename, "rb+", &database));

    long offset = ftell(database.file);

    VipsImage* in = NULL;
    fseek(coq_file, 0, SEEK_END);
    long size = ftell(coq_file);
    fseek(coq_file, 0, SEEK_SET);
    void* in_buffer = calloc(1, size);
    fread(in_buffer, size, 1, coq_file);
    vips_jpegload_buffer(in_buffer, size, &in, NULL);

    size_t out_len = 0;
    void* buffer = NULL;
    vips_jpegsave_buffer(in, &buffer, &out_len, NULL);
    
    fseek(database.file, sizeof(struct imgfs_header) + 3 * sizeof(struct img_metadata), SEEK_SET);
    fwrite(buffer, out_len, 1UL, database.file);

    database.header.nb_files++;
    database.header.version++;

    fseek(database.file, 0, SEEK_SET);

    fwrite(&database.header, sizeof(struct imgfs_header), 1UL, database.file);
    
    database.metadata[0].img_id[0] = 'd';
    database.metadata[0].img_id[1] = '\0';
    database.metadata[0].SHA[0] = '2';
    database.metadata[0].SHA[1] = '\0';
    
    database.metadata[0].is_valid = 1;
    database.metadata[0].offset[ORIG_RES] = offset;
    database.metadata[0].size[ORIG_RES] = out_len;
    unsigned int orig_res_new[ORIG_RES] = {1200, 800};
    memcpy(database.metadata[0].orig_res, orig_res_new, 2 * sizeof(unsigned int));

    fseek(database.file, sizeof(struct imgfs_header), SEEK_SET);
    fwrite(database.metadata, sizeof(struct img_metadata), 1UL, database.file);

    VipsImage* in_small = NULL;
    char* small_filename = "../data/pic1_small.jpg";
    FILE* coq_file_small = fopen(small_filename, "rb");
    fseek(coq_file_small, 0, SEEK_END);
    long small_size = ftell(coq_file_small);
    fseek(coq_file_small, 0, SEEK_SET);
    void* in_buffer_small = calloc(1, small_size);
    fread(in_buffer_small, small_size, 1, coq_file_small);
    vips_jpegload_buffer(in_buffer_small, small_size, &in_small, NULL);

    size_t out_len_small = 0;
    void* small_image_buffer_reference = NULL;
    vips_jpegsave_buffer(in_small, &small_image_buffer_reference, &out_len_small);

    ck_assert_err_none(lazily_resize(SMALL_RES, &database, 0));
    fseek(database.file, database.metadata[0].offset[SMALL_RES], SEEK_SET);

    void* small_image_buffer = calloc(1, out_len_small);
    fread(small_image_buffer, out_len_small, 1, database.file);

    ck_assert_int_eq(memcmp(small_image_buffer_reference, small_image_buffer, out_len_small), 0);

    g_object_unref(VIPS_OBJECT(in));
    g_object_unref(VIPS_OBJECT(in_small));

    free(buffer);
    free(small_image_buffer);
    free(in_buffer);
    free(in_buffer_small);
    free(small_image_buffer_reference);

    fclose(coq_file);
    fclose(coq_file_small);

    do_close(&database);

    end_test_print;
}
END_TEST

START_TEST(lazily_resize_valid_mure_small)
{
    start_test_print;

    VIPS_INIT("");

    char* filename = "../data/mure.jpg";

    FILE* coq_file = fopen(filename, "rb");

    struct imgfs_file database;
    const char* imgfs_filename = "test_01.imgfs";
    const char* max_files_flag = "-max_files";
    const char* max_files = "3";
    const char* thumb_res_flag = "-thumb_res";
    const char* thumb_res_width = "64";
    const char* thumb_res_height = "45";
    const char* small_res_flag = "-small_res";
    const char* small_res_width = "320";
    const char* small_res_height = "228";
    
    int argc = 9;
    char* argv_old[9] = {imgfs_filename, max_files_flag, max_files, thumb_res_flag, thumb_res_width, thumb_res_height, small_res_flag, small_res_width, small_res_height};
    char** argv = (char**) argv_old;

    ck_assert_err_none(do_create_cmd(argc, argv));
    ck_assert_err_none(do_open(imgfs_filename, "rb+", &database));

    long offset = ftell(database.file);

    VipsImage* in = NULL;
    fseek(coq_file, 0, SEEK_END);
    long size = ftell(coq_file);
    fseek(coq_file, 0, SEEK_SET);
    void* in_buffer = calloc(1, size);
    fread(in_buffer, size, 1, coq_file);
    vips_jpegload_buffer(in_buffer, size, &in, NULL);

    size_t out_len = 0;
    void* buffer = NULL;
    vips_jpegsave_buffer(in, &buffer, &out_len, NULL);
    
    fseek(database.file, sizeof(struct imgfs_header) + 3 * sizeof(struct img_metadata), SEEK_SET);
    fwrite(buffer, out_len, 1UL, database.file);

    database.header.nb_files++;
    database.header.version++;

    fseek(database.file, 0, SEEK_SET);

    fwrite(&database.header, sizeof(struct imgfs_header), 1UL, database.file);
    
    database.metadata[0].img_id[0] = 'd';
    database.metadata[0].img_id[1] = '\0';
    database.metadata[0].SHA[0] = '2';
    database.metadata[0].SHA[1] = '\0';
    
    database.metadata[0].is_valid = 1;
    database.metadata[0].offset[ORIG_RES] = offset;
    database.metadata[0].size[ORIG_RES] = out_len;
    unsigned int orig_res_new[ORIG_RES] = {640, 455};
    memcpy(database.metadata[0].orig_res, orig_res_new, 2 * sizeof(unsigned int));

    fseek(database.file, sizeof(struct imgfs_header), SEEK_SET);
    fwrite(database.metadata, sizeof(struct img_metadata), 1UL, database.file);

    VipsImage* in_small = NULL;
    char* small_filename = "../data/mure_small.jpg";
    FILE* coq_file_small = fopen(small_filename, "rb");
    fseek(coq_file_small, 0, SEEK_END);
    long small_size = ftell(coq_file_small);
    fseek(coq_file_small, 0, SEEK_SET);
    void* in_buffer_small = calloc(1, small_size);
    fread(in_buffer_small, small_size, 1, coq_file_small);
    vips_jpegload_buffer(in_buffer_small, small_size, &in_small, NULL);

    size_t out_len_small = 0;
    void* small_image_buffer_reference = NULL;
    vips_jpegsave_buffer(in_small, &small_image_buffer_reference, &out_len_small);

    ck_assert_err_none(lazily_resize(SMALL_RES, &database, 0));
    fseek(database.file, database.metadata[0].offset[SMALL_RES], SEEK_SET);

    void* small_image_buffer = calloc(1, out_len_small);
    fread(small_image_buffer, out_len_small, 1, database.file);

    ck_assert_int_eq(memcmp(small_image_buffer_reference, small_image_buffer, out_len_small), 0);

    g_object_unref(VIPS_OBJECT(in));
    g_object_unref(VIPS_OBJECT(in_small));

    free(buffer);
    free(small_image_buffer);
    free(in_buffer);
    free(in_buffer_small);
    free(small_image_buffer_reference);

    fclose(coq_file);
    fclose(coq_file_small);

    do_close(&database);

    end_test_print;
}
END_TEST

START_TEST(lazily_resize_valid_mure_thumb)
{
    start_test_print;

    VIPS_INIT("");

    char* filename = "../data/mure.jpg";

    FILE* coq_file = fopen(filename, "rb");

    struct imgfs_file database;
    const char* imgfs_filename = "test_01.imgfs";
    const char* max_files_flag = "-max_files";
    const char* max_files = "3";
    const char* thumb_res_flag = "-thumb_res";
    const char* thumb_res_width = "64";
    const char* thumb_res_height = "45";
    const char* small_res_flag = "-small_res";
    const char* small_res_width = "320";
    const char* small_res_height = "228";
    
    int argc = 9;
    char* argv_old[9] = {imgfs_filename, max_files_flag, max_files, thumb_res_flag, thumb_res_width, thumb_res_height, small_res_flag, small_res_width, small_res_height};
    char** argv = (char**) argv_old;

    ck_assert_err_none(do_create_cmd(argc, argv));
    ck_assert_err_none(do_open(imgfs_filename, "rb+", &database));

    long offset = ftell(database.file);

    VipsImage* in = NULL;
    fseek(coq_file, 0, SEEK_END);
    long size = ftell(coq_file);
    fseek(coq_file, 0, SEEK_SET);
    void* in_buffer = calloc(1, size);
    fread(in_buffer, size, 1, coq_file);
    vips_jpegload_buffer(in_buffer, size, &in, NULL);

    size_t out_len = 0;
    void* buffer = NULL;
    vips_jpegsave_buffer(in, &buffer, &out_len, NULL);
    
    fseek(database.file, sizeof(struct imgfs_header) + 3 * sizeof(struct img_metadata), SEEK_SET);
    fwrite(buffer, out_len, 1UL, database.file);

    database.header.nb_files++;
    database.header.version++;

    fseek(database.file, 0, SEEK_SET);

    fwrite(&database.header, sizeof(struct imgfs_header), 1UL, database.file);
    
    database.metadata[0].img_id[0] = 'd';
    database.metadata[0].img_id[1] = '\0';
    database.metadata[0].SHA[0] = '2';
    database.metadata[0].SHA[1] = '\0';
    
    database.metadata[0].is_valid = 1;
    database.metadata[0].offset[ORIG_RES] = offset;
    database.metadata[0].size[ORIG_RES] = out_len;
    unsigned int orig_res_new[ORIG_RES] = {640, 455};
    memcpy(database.metadata[0].orig_res, orig_res_new, 2 * sizeof(unsigned int));

    fseek(database.file, sizeof(struct imgfs_header), SEEK_SET);
    fwrite(database.metadata, sizeof(struct img_metadata), 1UL, database.file);

    VipsImage* in_small = NULL;
    char* small_filename = "../data/mure_thumb.jpg";
    FILE* coq_file_small = fopen(small_filename, "rb");
    fseek(coq_file_small, 0, SEEK_END);
    long small_size = ftell(coq_file_small);
    fseek(coq_file_small, 0, SEEK_SET);
    void* in_buffer_small = calloc(1, small_size);
    fread(in_buffer_small, small_size, 1, coq_file_small);
    vips_jpegload_buffer(in_buffer_small, small_size, &in_small, NULL);

    size_t out_len_small = 0;
    void* small_image_buffer_reference = NULL;
    vips_jpegsave_buffer(in_small, &small_image_buffer_reference, &out_len_small);

    ck_assert_err_none(lazily_resize(THUMB_RES, &database, 0));
    fseek(database.file, database.metadata[0].offset[THUMB_RES], SEEK_SET);

    void* small_image_buffer = calloc(1, out_len_small);
    fread(small_image_buffer, out_len_small, 1, database.file);

    ck_assert_int_eq(memcmp(small_image_buffer_reference, small_image_buffer, out_len_small), 0);

    g_object_unref(VIPS_OBJECT(in));
    g_object_unref(VIPS_OBJECT(in_small));

    free(buffer);
    free(small_image_buffer);
    free(in_buffer);
    free(in_buffer_small);
    free(small_image_buffer_reference);

    fclose(coq_file);
    fclose(coq_file_small);

    do_close(&database);

    end_test_print;
}
END_TEST


START_TEST(lazily_resize_valid_brouillard_thumb)
{
    start_test_print;

    VIPS_INIT("");

    char* filename = "../data/brouillard.jpg";

    FILE* coq_file = fopen(filename, "rb");

    struct imgfs_file database;
    const char* imgfs_filename = "test_01.imgfs";
    const char* max_files_flag = "-max_files";
    const char* max_files = "3";
    const char* thumb_res_flag = "-thumb_res";
    const char* thumb_res_width = "60";
    const char* thumb_res_height = "40";
    const char* small_res_flag = "-small_res";
    const char* small_res_width = "300";
    const char* small_res_height = "200";
    
    int argc = 9;
    char* argv_old[9] = {imgfs_filename, max_files_flag, max_files, thumb_res_flag, thumb_res_width, thumb_res_height, small_res_flag, small_res_width, small_res_height};
    char** argv = (char**) argv_old;

    ck_assert_err_none(do_create_cmd(argc, argv));
    ck_assert_err_none(do_open(imgfs_filename, "rb+", &database));

    long offset = ftell(database.file);

    VipsImage* in = NULL;
    fseek(coq_file, 0, SEEK_END);
    long size = ftell(coq_file);
    fseek(coq_file, 0, SEEK_SET);
    void* in_buffer = calloc(1, size);
    fread(in_buffer, size, 1, coq_file);
    vips_jpegload_buffer(in_buffer, size, &in, NULL);

    size_t out_len = 0;
    void* buffer = NULL;
    vips_jpegsave_buffer(in, &buffer, &out_len, NULL);
    
    fseek(database.file, sizeof(struct imgfs_header) + 3 * sizeof(struct img_metadata), SEEK_SET);
    fwrite(buffer, out_len, 1UL, database.file);

    database.header.nb_files++;
    database.header.version++;

    fseek(database.file, 0, SEEK_SET);

    fwrite(&database.header, sizeof(struct imgfs_header), 1UL, database.file);
    
    database.metadata[0].img_id[0] = 'd';
    database.metadata[0].img_id[1] = '\0';
    database.metadata[0].SHA[0] = '2';
    database.metadata[0].SHA[1] = '\0';
    
    database.metadata[0].is_valid = 1;
    database.metadata[0].offset[ORIG_RES] = offset;
    database.metadata[0].size[ORIG_RES] = out_len;
    unsigned int orig_res_new[ORIG_RES] = {600, 400};
    memcpy(database.metadata[0].orig_res, orig_res_new, 2 * sizeof(unsigned int));

    fseek(database.file, sizeof(struct imgfs_header), SEEK_SET);
    fwrite(database.metadata, sizeof(struct img_metadata), 1UL, database.file);

    VipsImage* in_small = NULL;
    char* small_filename = "../data/brouillard_thumb.jpg";
    FILE* coq_file_small = fopen(small_filename, "rb");
    fseek(coq_file_small, 0, SEEK_END);
    long small_size = ftell(coq_file_small);
    fseek(coq_file_small, 0, SEEK_SET);
    void* in_buffer_small = calloc(1, small_size);
    fread(in_buffer_small, small_size, 1, coq_file_small);
    vips_jpegload_buffer(in_buffer_small, small_size, &in_small, NULL);

    size_t out_len_small = 0;
    void* small_image_buffer_reference = NULL;
    vips_jpegsave_buffer(in_small, &small_image_buffer_reference, &out_len_small);

    ck_assert_err_none(lazily_resize(THUMB_RES, &database, 0));
    fseek(database.file, database.metadata[0].offset[THUMB_RES], SEEK_SET);

    void* small_image_buffer = calloc(1, out_len_small);
    fread(small_image_buffer, out_len_small, 1, database.file);

    ck_assert_int_eq(memcmp(small_image_buffer_reference, small_image_buffer, out_len_small), 0);

    g_object_unref(VIPS_OBJECT(in));
    g_object_unref(VIPS_OBJECT(in_small));

    free(buffer);
    free(small_image_buffer);
    free(in_buffer);
    free(in_buffer_small);
    free(small_image_buffer_reference);

    fclose(coq_file);
    fclose(coq_file_small);

    do_close(&database);

    end_test_print;
}
END_TEST


START_TEST(lazily_resize_valid_brouillard_small)
{
    start_test_print;

    VIPS_INIT("");

    char* filename = "../data/brouillard.jpg";

    FILE* coq_file = fopen(filename, "rb");

    struct imgfs_file database;
    const char* imgfs_filename = "test_01.imgfs";
    const char* max_files_flag = "-max_files";
    const char* max_files = "3";
    const char* thumb_res_flag = "-thumb_res";
    const char* thumb_res_width = "60";
    const char* thumb_res_height = "40";
    const char* small_res_flag = "-small_res";
    const char* small_res_width = "300";
    const char* small_res_height = "200";
    
    int argc = 9;
    char* argv_old[9] = {imgfs_filename, max_files_flag, max_files, thumb_res_flag, thumb_res_width, thumb_res_height, small_res_flag, small_res_width, small_res_height};
    char** argv = (char**) argv_old;

    ck_assert_err_none(do_create_cmd(argc, argv));
    ck_assert_err_none(do_open(imgfs_filename, "rb+", &database));

    long offset = ftell(database.file);

    VipsImage* in = NULL;
    fseek(coq_file, 0, SEEK_END);
    long size = ftell(coq_file);
    fseek(coq_file, 0, SEEK_SET);
    void* in_buffer = calloc(1, size);
    fread(in_buffer, size, 1, coq_file);
    vips_jpegload_buffer(in_buffer, size, &in, NULL);

    size_t out_len = 0;
    void* buffer = NULL;
    vips_jpegsave_buffer(in, &buffer, &out_len, NULL);
    
    fseek(database.file, sizeof(struct imgfs_header) + 3 * sizeof(struct img_metadata), SEEK_SET);
    fwrite(buffer, out_len, 1UL, database.file);

    database.header.nb_files++;
    database.header.version++;

    fseek(database.file, 0, SEEK_SET);

    fwrite(&database.header, sizeof(struct imgfs_header), 1UL, database.file);
    
    database.metadata[0].img_id[0] = 'd';
    database.metadata[0].img_id[1] = '\0';
    database.metadata[0].SHA[0] = '2';
    database.metadata[0].SHA[1] = '\0';
    
    database.metadata[0].is_valid = 1;
    database.metadata[0].offset[ORIG_RES] = offset;
    database.metadata[0].size[ORIG_RES] = out_len;
    unsigned int orig_res_new[ORIG_RES] = {600, 400};
    memcpy(database.metadata[0].orig_res, orig_res_new, 2 * sizeof(unsigned int));

    fseek(database.file, sizeof(struct imgfs_header), SEEK_SET);
    fwrite(database.metadata, sizeof(struct img_metadata), 1UL, database.file);

    VipsImage* in_small = NULL;
    char* small_filename = "../data/brouillard_small.jpg";
    FILE* coq_file_small = fopen(small_filename, "rb");
    fseek(coq_file_small, 0, SEEK_END);
    long small_size = ftell(coq_file_small);
    fseek(coq_file_small, 0, SEEK_SET);
    void* in_buffer_small = calloc(1, small_size);
    fread(in_buffer_small, small_size, 1, coq_file_small);
    vips_jpegload_buffer(in_buffer_small, small_size, &in_small, NULL);

    size_t out_len_small = 0;
    void* small_image_buffer_reference = NULL;
    vips_jpegsave_buffer(in_small, &small_image_buffer_reference, &out_len_small);

    ck_assert_err_none(lazily_resize(SMALL_RES, &database, 0));
    fseek(database.file, database.metadata[0].offset[SMALL_RES], SEEK_SET);

    void* small_image_buffer = calloc(1, out_len_small);
    fread(small_image_buffer, out_len_small, 1, database.file);

    ck_assert_int_eq(memcmp(small_image_buffer_reference, small_image_buffer, out_len_small), 0);

    g_object_unref(VIPS_OBJECT(in));
    g_object_unref(VIPS_OBJECT(in_small));

    free(buffer);
    free(small_image_buffer);
    free(in_buffer);
    free(in_buffer_small);
    free(small_image_buffer_reference);

    fclose(coq_file);
    fclose(coq_file_small);

    do_close(&database);

    end_test_print;
}
END_TEST

// ======================================================================
Suite *imgfs_content_test_suite()
{
    Suite *s = suite_create("Tests imgfs_content implementation");

    Add_Test(s, lazily_resize_null_params);
    Add_Test(s, lazily_resize_out_of_range_index);
    Add_Test(s, lazily_resize_empty_index);
    Add_Test(s, lazily_resize_res_orig);
    Add_Test(s, lazily_resize_invalid_mode);
    Add_Test(s, lazily_resize_already_exists);
    Add_Test(s, lazily_resize_valid);
    Add_Test(s, lazily_resize_valid_coquelicot_small);
    Add_Test(s, lazily_resize_valid_coquelicots_thumbnail);
    Add_Test(s, lazily_resize_valid_papillon_thumbnail);
    Add_Test(s, lazily_resize_valid_pic1_small);
    Add_Test(s, lazily_resize_valid_mure_small);
    Add_Test(s, lazily_resize_valid_mure_thumb);
    Add_Test(s, lazily_resize_valid_brouillard_small);
    Add_Test(s, lazily_resize_valid_brouillard_thumb);

    return s;
}

TEST_SUITE_VIPS(imgfs_content_test_suite)
