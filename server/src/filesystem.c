//
// Created by hanyuan on 2024/10/10.
//

#include "filesystem.h"
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>

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
    char *backward = malloc(len);
    strcpy(backward, path);
    if (len == 1) {
        return backward;
    }
    if (backward[len - 1] == '/') {
        backward[len - 1] = 0;
        len--;
    }
    for (size_t i = len - 1; i >= 0; i--) {
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
