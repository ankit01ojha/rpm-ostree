/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2017 Colin Walters <walters@verbum.org>
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

#include "config.h"

#include "string.h"

#include "rpmostree-origin.h"

struct RpmOstreeOrigin {
  guint refcount;

  /* this is the single source of truth */
  GKeyFile *kf;

  char *cached_refspec;
  char *cached_override_commit;
  char *cached_unconfigured_state;
  char **cached_initramfs_args;
  GHashTable *cached_packages;
};

static GKeyFile *
keyfile_dup (GKeyFile *kf)
{
  GKeyFile *ret = g_key_file_new ();
  gsize length = 0;
  g_autofree char *data = g_key_file_to_data (kf, &length, NULL);

  g_key_file_load_from_data (ret, data, length, G_KEY_FILE_KEEP_COMMENTS, NULL);
  return ret;
}

RpmOstreeOrigin *
rpmostree_origin_parse_keyfile (GKeyFile         *origin,
                                GError          **error)
{
  g_autoptr(RpmOstreeOrigin) ret = NULL;

  ret = g_new0 (RpmOstreeOrigin, 1);
  ret->refcount = 1;
  ret->kf = keyfile_dup (origin);

  ret->cached_packages = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                g_free, NULL);

  /* NOTE hack here - see https://github.com/ostreedev/ostree/pull/343 */
  g_key_file_remove_key (ret->kf, "origin", "unlocked", NULL);

  ret->cached_unconfigured_state = g_key_file_get_string (ret->kf, "origin", "unconfigured-state", NULL);

  ret->cached_refspec = g_key_file_get_string (ret->kf, "origin", "refspec", NULL);
  if (!ret->cached_refspec)
    {
      g_auto(GStrv) packages = NULL;

      ret->cached_refspec = g_key_file_get_string (ret->kf, "origin", "baserefspec", NULL);
      if (!ret->cached_refspec)
        {
          g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                       "No origin/refspec or origin/baserefspec in current deployment origin; cannot handle via rpm-ostree");
          return NULL;
        }

      packages = g_key_file_get_string_list (ret->kf, "packages", "requested", NULL, NULL);
      for (char **it = packages; it && *it; it++)
        g_hash_table_add (ret->cached_packages, g_steal_pointer (it));
    }

  ret->cached_override_commit =
    g_key_file_get_string (ret->kf, "origin", "override-commit", NULL);

  ret->cached_initramfs_args =
    g_key_file_get_string_list (ret->kf, "rpmostree", "initramfs-args", NULL, NULL);

  return g_steal_pointer (&ret);
}

RpmOstreeOrigin *
rpmostree_origin_dup (RpmOstreeOrigin *origin)
{
  return rpmostree_origin_parse_keyfile (origin->kf, NULL);
}

const char *
rpmostree_origin_get_refspec (RpmOstreeOrigin *origin)
{
  return origin->cached_refspec;
}

GHashTable *
rpmostree_origin_get_packages (RpmOstreeOrigin *origin)
{
  return origin->cached_packages;
}

const char *
rpmostree_origin_get_override_commit (RpmOstreeOrigin *origin)
{
  return origin->cached_override_commit;
}

gboolean
rpmostree_origin_get_regenerate_initramfs (RpmOstreeOrigin *origin)
{
  return g_key_file_get_boolean (origin->kf, "rpmostree", "regenerate-initramfs", NULL);
}

const char *const*
rpmostree_origin_get_initramfs_args (RpmOstreeOrigin *origin)
{
  return (const char * const*)origin->cached_initramfs_args;
}

const char*
rpmostree_origin_get_unconfigured_state (RpmOstreeOrigin *origin)
{
  return origin->cached_unconfigured_state;
}

GKeyFile *
rpmostree_origin_dup_keyfile (RpmOstreeOrigin *origin)
{
  return keyfile_dup (origin->kf);
}

char *
rpmostree_origin_get_string (RpmOstreeOrigin *origin,
                             const char *section,
                             const char *value)
{
  return g_key_file_get_string (origin->kf, section, value, NULL);
}

RpmOstreeOrigin*
rpmostree_origin_ref (RpmOstreeOrigin *origin)
{
  g_assert (origin);
  origin->refcount++;
  return origin;
}

void
rpmostree_origin_unref (RpmOstreeOrigin *origin)
{
  g_assert (origin);
  g_assert_cmpint (origin->refcount, >, 0);
  origin->refcount--;
  if (origin->refcount > 0)
    return;
  g_key_file_unref (origin->kf);
  g_free (origin->cached_refspec);
  g_free (origin->cached_unconfigured_state);
  g_strfreev (origin->cached_initramfs_args);
  g_clear_pointer (&origin->cached_packages, g_hash_table_unref);
  g_free (origin);
}

void
rpmostree_origin_set_regenerate_initramfs (RpmOstreeOrigin *origin,
                                           gboolean regenerate,
                                           char **args)
{
  const char *section = "rpmostree";
  const char *regeneratek = "regenerate-initramfs";
  const char *argsk = "initramfs-args";

  g_strfreev (origin->cached_initramfs_args);

  if (regenerate)
    {
      g_key_file_set_boolean (origin->kf, section, regeneratek, TRUE);
      if (args && *args)
        {
          g_key_file_set_string_list (origin->kf, section, argsk,
                                      (const char *const*)args,
                                      g_strv_length (args));
        }
      else
        g_key_file_remove_key (origin->kf, section, argsk, NULL);
    }
  else
    {
      g_key_file_remove_key (origin->kf, section, regeneratek, NULL);
      g_key_file_remove_key (origin->kf, section, argsk, NULL);
    }

  origin->cached_initramfs_args =
    g_key_file_get_string_list (origin->kf, "rpmostree", "initramfs-args",
                                NULL, NULL);
}

void
rpmostree_origin_set_override_commit (RpmOstreeOrigin *origin,
                                      const char      *checksum,
                                      const char      *version)
{
  if (checksum != NULL)
    {
      g_key_file_set_string (origin->kf, "origin", "override-commit", checksum);

      /* Add a comment with the version, to be nice. */
      if (version != NULL)
        {
          g_autofree char *comment =
            g_strdup_printf ("Version %s [%.10s]", version, checksum);
          g_key_file_set_comment (origin->kf, "origin", "override-commit", comment, NULL);
        }
    }
  else
    {
      g_key_file_remove_key (origin->kf, "origin", "override-commit", NULL);
    }

  g_free (origin->cached_override_commit);
  origin->cached_override_commit = g_strdup (checksum);
}

/* updates an origin's refspec without migrating format */
static gboolean
origin_set_refspec (GKeyFile   *origin,
                    const char *new_refspec,
                    GError    **error)
{
  if (g_key_file_has_key (origin, "origin", "baserefspec", error))
    {
      g_key_file_set_value (origin, "origin", "baserefspec", new_refspec);
      return TRUE;
    }

  if (error && *error)
    return FALSE;

  g_key_file_set_value (origin, "origin", "refspec", new_refspec);
  return TRUE;
}

gboolean
rpmostree_origin_set_rebase (RpmOstreeOrigin *origin,
                             const char      *new_refspec,
                             GError         **error)
{
  if (!origin_set_refspec (origin->kf, new_refspec, error))
    return FALSE;

  /* we don't want to carry any commit overrides during a rebase */
  rpmostree_origin_set_override_commit (origin, NULL, NULL);

  g_free (origin->cached_refspec);
  origin->cached_refspec = g_strdup (new_refspec);

  return TRUE;
}

static void
update_keyfile_pkgs_from_cache (RpmOstreeOrigin *origin)
{
  /* we're abusing a bit the concept of cache here, though
   * it's just easier to go from cache to origin */
  g_autofree char **pkgv =
    (char**) g_hash_table_get_keys_as_array (origin->cached_packages, NULL);
  g_key_file_set_string_list (origin->kf, "packages", "requested",
                              (const char *const*)pkgv, g_strv_length (pkgv));

  if (g_hash_table_size (origin->cached_packages) > 0)
    {
      /* migrate to baserefspec model */
      g_key_file_set_value (origin->kf, "origin", "baserefspec",
                            origin->cached_refspec);
      g_key_file_remove_key (origin->kf, "origin", "refspec", NULL);
    }
}

gboolean
rpmostree_origin_add_packages (RpmOstreeOrigin   *origin,
                               char             **packages,
                               GCancellable      *cancellable,
                               GError           **error)
{
  gboolean changed = FALSE;

  for (char **it = packages; it && *it; it++)
    {
      /* The list of packages is really a list of provides, so string equality
       * is a bit weird here. Multiple provides can resolve to the same package
       * and we allow that. But still, let's make sure that silly users don't
       * request the exact string. */
      if (g_hash_table_contains (origin->cached_packages, *it))
        {
          g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                       "Package/capability '%s' is already requested", *it);
          return FALSE;
        }
      g_hash_table_add (origin->cached_packages, g_strdup (*it));
      changed = TRUE;
    }

  if (changed)
    update_keyfile_pkgs_from_cache (origin);

  return TRUE;
}

gboolean
rpmostree_origin_delete_packages (RpmOstreeOrigin  *origin,
                                  char            **packages,
                                  GCancellable     *cancellable,
                                  GError          **error)
{
  gboolean changed = FALSE;

  for (char **it = packages; it && *it; it++)
    {
      if (!g_hash_table_contains (origin->cached_packages, *it))
        {
          g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                       "Package/capability '%s' is not currently requested", *it);
          return FALSE;
        }
      g_hash_table_remove (origin->cached_packages, *it);
      changed = TRUE;
    }

  if (changed)
    update_keyfile_pkgs_from_cache (origin);

  return TRUE;
}
