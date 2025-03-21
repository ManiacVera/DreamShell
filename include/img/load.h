
#ifndef __IMG_LOAD_H
#define __IMG_LOAD_H

/* Load PVR to a KOS Platform Independent Image */
int pvr_to_img(const char *filename, kos_img_t *rv);

/* Load zlib compressed KMG to a KOS Platform Independent Image */
int gzip_kmg_to_img(const char * filename, kos_img_t *rv);

#endif // __IMG_LOAD_H
