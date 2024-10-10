//
// Created by hanyuan on 2024/10/10.
//

#ifndef SERVER_FILESYSTEM_H
#define SERVER_FILESYSTEM_H

#define PATH_SEPARATOR '/'

int fs_directory_exists(const char *path);

/* 这个路径需要 free 掉！ */
const char *fs_path_join(const char *path1, const char *path2);

#endif //SERVER_FILESYSTEM_H
