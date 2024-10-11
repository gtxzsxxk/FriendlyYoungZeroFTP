//
// Created by hanyuan on 2024/10/10.
//

#ifndef SERVER_FILESYSTEM_H
#define SERVER_FILESYSTEM_H

#include <string.h>

#define PATH_SEPARATOR '/'

int fs_directory_exists(const char *path);

int fs_directory_allows(const char *root, const char *path);

const char *fs_path_backward(const char *path);

/* 这个路径需要 free 掉！ */
const char *fs_path_join(const char *path1, const char *path2);

const char *fs_path_erase(const char *root, const char *path, char **free_handle);

int fs_file_exists(const char *filename);

const char *fs_get_filename(const char *path);

size_t fs_get_file_size(const char *path);

#endif //SERVER_FILESYSTEM_H
