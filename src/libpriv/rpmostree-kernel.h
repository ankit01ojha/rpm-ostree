/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2013,2014,2017 Colin Walters <walters@verbum.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2 of the licence or (at
 * your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#pragma once

#include <ostree.h>

GVariant *
rpmostree_find_kernel (int rootfs_dfd,
                       GCancellable *cancellable,
                       GError **error);

gboolean
rpmostree_finalize_kernel (int rootfs_dfd,
                           const char *bootdir,
                           const char *kver,
                           const char *kernel_path,
                           const char *initramfs_tmp_path,
                           int         initramfs_tmp_fd, /* consumed */
                           GCancellable *cancellable,
                           GError **error);

gboolean
rpmostree_run_dracut (int     rootfs_dfd,
                      const char *const* argv,
                      const char *rebuild_from_initramfs,
                      int     *out_initramfs_tmpfd,
                      char   **out_initramfs_tmppath,
                      GCancellable  *cancellable,
                      GError **error);
