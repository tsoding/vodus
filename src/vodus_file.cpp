int create_directory_if_not_exists(const char *dirpath)
{
    struct stat statbuf = {};
    int res = stat(dirpath, &statbuf);

    if (res == -1) {
        if (errno == ENOENT) {
            // TODO: create_directory_if_not_exists does not create parent folders
            return mkdir(dirpath, 0755);
        } else {
            return -1;
        }
    }

    if ((statbuf.st_mode & S_IFMT) != S_IFDIR) {
        errno = ENOTDIR;
        return -1;
    }

    return 0;
}
