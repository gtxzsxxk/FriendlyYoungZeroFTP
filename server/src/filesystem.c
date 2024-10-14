//
// Created by hanyuan on 2024/10/10.
//

#include "filesystem.h"
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>

int fs_directory_exists(const char *path) {
    struct stat info;
    if (stat(path, &info) != 0) {
        return 0;
    }
    return (info.st_mode & S_IFDIR) != 0;
}

int fs_directory_allows(const char *root, const char *path) {
    char tmp_path[256];
    strcpy(tmp_path, path);
    size_t len1 = strlen(path);
    if (tmp_path[len1 - 1] == '/' && len1 > 1) {
        tmp_path[len1 - 1] = 0;
        len1--;
    }
    char tmp_root[256];
    strcpy(tmp_root, root);
    size_t len2 = strlen(root);
    if (tmp_root[len2 - 1] == '/' && len2 > 1) {
        tmp_root[len2 - 1] = 0;
        len2--;
    }

    if (len1 < len2) {
        return 0;
    }
    return !strncmp(tmp_root, tmp_path, len2);
}

const char *fs_path_join(const char *path1, const char *path2) {
    if (!path1 || !path2) {
        return NULL;
    }
    size_t len1 = strlen(path1);
    size_t len2 = strlen(path2);

    int needs_separator = (path1[len1 - 1] != PATH_SEPARATOR) && (path2[0] != PATH_SEPARATOR);
    char *result = malloc(len1 + len2 + (needs_separator ? 2 : 1));
    if (!result) {
        return NULL;
    }
    strcpy(result, path1);
    if (needs_separator) {
        result[len1] = PATH_SEPARATOR;
        len1++;
    }
    strcpy(result + len1, path2);
    return result;
}

const char *fs_path_backward(const char *path) {
    size_t len = strlen(path);
    char *backward = malloc(len + 10);
    strcpy(backward, path);
    if (len == 1) {
        return backward;
    }
    if (backward[len - 1] == '/') {
        backward[len - 1] = 0;
        len--;
    }
    for (ssize_t i = len - 1; i >= 0; i--) {
        if (backward[i] == '/' && i > 0) {
            backward[i] = 0;
            break;
        }
        backward[i] = 0;
    }

    return backward;
}

const char *fs_path_erase(const char *root, const char *path, char **free_handle) {
    char *tmp_path = malloc(256);
    strcpy(tmp_path, path);
    size_t len1 = strlen(path);
    if (tmp_path[len1 - 1] != '/') {
        tmp_path[len1] = '/';
        tmp_path[len1 + 1] = 0;
        len1++;
    }
    char tmp_root[256];
    strcpy(tmp_root, root);
    size_t len2 = strlen(root);
    if (tmp_root[len2 - 1] == '/' && len2 > 1) {
        tmp_root[len2 - 1] = 0;
        len2--;
    }

    if (len1 < len2) {
        return NULL;
    }

    *free_handle = tmp_path;
    return tmp_path + len2;
}

int fs_file_exists(const char *filename) {
    return access(filename, F_OK) == 0;
}

const char *fs_get_filename(const char *path) {
    const char *last_slash = strrchr(path, '/');
    if (last_slash) {
        return last_slash + 1;
    } else {
        return path;
    }
}

size_t fs_get_file_size(const char *path) {
    struct stat st;

    if (stat(path, &st) == 0) {
        return st.st_size;
    } else {
        return -1;
    }
}

void fs_get_directory(const char *file_path, char *directory_path) {
    // 复制文件路径到目标缓冲区
    strcpy(directory_path, file_path);

    // 找到最后一个 '/'
    char *last_slash = strrchr(directory_path, '/');

    // 如果找到了 '/'
    if (last_slash != NULL) {
        *last_slash = '\0'; // 将最后一个 '/' 替换为终止符 '\0'
    } else {
        // 如果路径中没有 '/'，说明它没有目录部分
        strcpy(directory_path, ".");
    }
}
