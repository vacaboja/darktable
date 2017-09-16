/*
    This file is part of darktable,
    copyright (c) 2010 henrik andersson.
    copyright (c) 2010--2017 tobias ellinghaus.

    darktable is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    darktable is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with darktable.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "common/variables.h"
#include "common/colorlabels.h"
#include "common/darktable.h"
#include "common/file_location.h"
#include "common/image.h"
#include "common/image_cache.h"
#include "common/metadata.h"
#include "common/utility.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

typedef struct dt_variables_data_t
{
  /** expanded result string */
  gchar *result;
  struct tm time;
  time_t exif_time;
  guint sequence;

  /** cached values that shouldn't change between variables in the same expansion process */
  char *homedir;
  char *pictures_folder;
  const char *file_ext;

  gboolean have_exif_tm;
  int exif_iso;
  char *camera_maker;
  char *camera_alias;
  int version;
  int stars;
  struct tm exif_tm;

} dt_variables_data_t;

static inline gboolean has_prefix(const char *str, const char *prefix, size_t *length)
{
  gboolean res = g_str_has_prefix(str, prefix);
  if(res) *length = strlen(prefix);
  return res;
}

static char *variable_get_base(dt_variables_params_t *params, char *variable, size_t *length)
{
  char *result = NULL;
  *length = 0;

  struct tm exif_tm = params->data->have_exif_tm ? params->data->exif_tm : params->data->time;

  if(has_prefix(variable, "YEAR", length))
    result = g_strdup_printf("%.4d", params->data->time.tm_year + 1900);
  else if(has_prefix(variable, "MONTH", length))
    result = g_strdup_printf("%.2d", params->data->time.tm_mon + 1);
  else if(has_prefix(variable, "DAY", length))
    result = g_strdup_printf("%.2d", params->data->time.tm_mday);
  else if(has_prefix(variable, "HOUR", length))
    result = g_strdup_printf("%.2d", params->data->time.tm_hour);
  else if(has_prefix(variable, "MINUTE", length))
    result = g_strdup_printf("%.2d", params->data->time.tm_min);
  else if(has_prefix(variable, "SECOND", length))
    result = g_strdup_printf("%.2d", params->data->time.tm_sec);

  else if(has_prefix(variable, "EXIF_YEAR", length))
    result = g_strdup_printf("%.4d", exif_tm.tm_year + 1900);
  else if(has_prefix(variable, "EXIF_MONTH", length))
    result = g_strdup_printf("%.2d", exif_tm.tm_mon + 1);
  else if(has_prefix(variable, "EXIF_DAY", length))
    result = g_strdup_printf("%.2d", exif_tm.tm_mday);
  else if(has_prefix(variable, "EXIF_HOUR", length))
    result = g_strdup_printf("%.2d", exif_tm.tm_hour);
  else if(has_prefix(variable, "EXIF_MINUTE", length))
    result = g_strdup_printf("%.2d", exif_tm.tm_min);
  else if(has_prefix(variable, "EXIF_SECOND", length))
    result = g_strdup_printf("%.2d", exif_tm.tm_sec);
  else if(has_prefix(variable, "EXIF_ISO", length))
    result = g_strdup_printf("%d", params->data->exif_iso);
  else if(has_prefix(variable, "MAKER", length))
    result = g_strdup(params->data->camera_maker);
  else if(has_prefix(variable, "MODEL", length))
    result = g_strdup(params->data->camera_alias);
  else if(has_prefix(variable, "ID", length))
    result = g_strdup_printf("%d", params->imgid);
  else if(has_prefix(variable, "VERSION", length))
    result = g_strdup_printf("%d", params->data->version);
  else if(has_prefix(variable, "JOBCODE", length))
    result = g_strdup(params->jobcode);
  else if(has_prefix(variable, "ROLL_NAME", length))
  {
    if(params->filename)
    {
      gchar *dirname = g_path_get_dirname(params->filename);
      result = g_path_get_basename(dirname);
      g_free(dirname);
    }
  }
  else if(has_prefix(variable, "FILE_DIRECTORY", length))
  {
    if(params->filename)
      result = g_path_get_dirname(params->filename);
  } // undocumented : backward compatibility
  else if(has_prefix(variable, "FILE_FOLDER", length))
  {
    if(params->filename)
      result = g_path_get_dirname(params->filename);
  }
  else if(has_prefix(variable, "FILE_NAME", length))
  {
    if(params->filename)
    {
      result = g_path_get_basename(params->filename);
      char *dot = g_strrstr(result, ".");
      if(dot) *dot = '\0';
    }
  }
  else if(has_prefix(variable, "FILE_EXTENSION", length))
    result = g_strdup(params->data->file_ext);
  else if(has_prefix(variable, "SEQUENCE", length))
    result = g_strdup_printf("%.4d", params->sequence >= 0 ? params->sequence : params->data->sequence);
  else if(has_prefix(variable, "USERNAME", length))
    result = g_strdup(g_get_user_name());
  else if(has_prefix(variable, "HOME_FOLDER", length))
    result = g_strdup(params->data->homedir); // undocumented : backward compatibility
  else if(has_prefix(variable, "HOME", length))
    result = g_strdup(params->data->homedir);
  else if(has_prefix(variable, "PICTURES_FOLDER", length))
    result = g_strdup(params->data->pictures_folder);
  else if(has_prefix(variable, "DESKTOP_FOLDER", length))
    result = g_strdup(g_get_user_special_dir(G_USER_DIRECTORY_DESKTOP)); // undocumented : backward compatibility
  else if(has_prefix(variable, "DESKTOP", length))
    result = g_strdup(g_get_user_special_dir(G_USER_DIRECTORY_DESKTOP));
  else if(has_prefix(variable, "STARS", length))
    result = g_strdup_printf("%d", params->data->stars);
  else if(has_prefix(variable, "LABELS", length))
  {
    // TODO: currently we concatenate all the color labels with a ',' as a separator. Maybe it's better to
    // only use the first/last label?
    GList *res = dt_metadata_get(params->imgid, "Xmp.darktable.colorlabels", NULL);
    res = g_list_first(res);
    if(res != NULL)
    {
      GList *labels = NULL;
      do
      {
        labels = g_list_append(labels, (char *)(_(dt_colorlabels_to_string(GPOINTER_TO_INT(res->data)))));
      } while((res = g_list_next(res)) != NULL);
      result = dt_util_glist_to_str(",", labels);
      g_list_free(labels);
    }
    g_list_free(res);
  }
  else if(has_prefix(variable, "TITLE", length))
  {
    GList *res = dt_metadata_get(params->imgid, "Xmp.dc.title", NULL);
    res = g_list_first(res);
    if(res != NULL)
    {
      result = g_strdup((char *)res->data);
    }
    g_list_free_full(res, &g_free);
  }
  else if(has_prefix(variable, "CREATOR", length))
  {
    GList *res = dt_metadata_get(params->imgid, "Xmp.dc.creator", NULL);
    res = g_list_first(res);
    if(res != NULL)
    {
      result = g_strdup((char *)res->data);
    }
    g_list_free_full(res, &g_free);
  }
  else if(has_prefix(variable, "PUBLISHER", length))
  {
    GList *res = dt_metadata_get(params->imgid, "Xmp.dc.publisher", NULL);
    res = g_list_first(res);
    if(res != NULL)
    {
      result = g_strdup((char *)res->data);
    }
    g_list_free_full(res, &g_free);
  }
  else if(has_prefix(variable, "RIGHTS", length))
  {
    GList *res = dt_metadata_get(params->imgid, "Xmp.dc.rights", NULL);
    res = g_list_first(res);
    if(res != NULL)
    {
      result = g_strdup((char *)res->data);
    }
    g_list_free_full(res, &g_free);
  }

  return result;
}

// bash style variable manipulation. all patterns are just simple string comparisons!
// See here for bash examples and documentation:
// http://www.tldp.org/LDP/abs/html/parameter-substitution.html
// https://www.gnu.org/software/bash/manual/html_node/Shell-Parameter-Expansion.html
static char *variable_get_value(dt_variables_params_t *params, char *variable_start, size_t variable_length)
{
  // in theory this should never happen
  if(!variable_start || variable_length == 3 || variable_start[0] != '$' || variable_start[1] != '(' ||
    variable_start[variable_length - 1] != ')')
    return g_strndup(variable_start, variable_length);

  // we want a copy we can mess with, change, ...
  char *variable = g_strndup(variable_start + 2, variable_length - 3);

  // first get the value of the variable
  size_t skip;
  char *base_value = variable_get_base(params, variable, &skip);
  char *substitution = variable + skip;

  // ... and now see if we have to change it
  if(*substitution == '\0')
  {
    // nothing to do, everything is fine!
  }
  else if(*substitution == '-')
  {
    /*
      $(parameter-default)
        If parameter not set, use default.
    */

    if(!base_value || !*base_value)
    {
      substitution++;
      g_free(base_value);
      base_value = g_strdup(substitution);
    }
  }
  else if(base_value)
  {
    const size_t base_value_length = strlen(base_value);
    if(*substitution == '+')
    {
      /*
        $(parameter+alt_value)
          If parameter set, use alt_value, else use null string.
      */

      if(*base_value)
      {
        substitution++;
        g_free(base_value);
        base_value = g_strdup(substitution);
      }
    }
    else if(*substitution == ':')
    {
      /*
        $(var:offset)
          Variable var expanded, starting from offset.

        $(var:offset:length)
          Expansion to a max of length characters of variable var, from offset.

        If offset evaluates to a number less than zero, the value is used as an offset in characters from the
        end of the value of parameter. If length evaluates to a number less than zero, it is interpreted as an
        offset in characters from the end of the value of parameter rather than a number of characters, and the
        expansion is the characters between offset and that result.
      */

      substitution++;
      char *colon = g_strstr_len(substitution, -1, ":");
      if(colon) *colon = '\0';

      const size_t base_value_utf8_length = g_utf8_strlen(base_value, -1);
      const int offset = atoi(substitution);
      char *start; // from where to copy
      char *end = base_value + base_value_length; // ... and until where

      // find where to start
      if(offset >= 0)
        start = g_utf8_offset_to_pointer(base_value, MIN(offset, base_value_utf8_length));
      else
        start = g_utf8_offset_to_pointer(base_value + base_value_length, MAX(offset, -1 * base_value_utf8_length));

      // now find the end if there is a length provided
      if(start && colon)
      {
        colon++;
        const size_t start_utf8_length = g_utf8_strlen(start, -1);
        const int length = atoi(colon);
        if(length >= 0)
          end = g_utf8_offset_to_pointer(start, MIN(length, start_utf8_length));
        else
          end = g_utf8_offset_to_pointer(base_value + base_value_length, MAX(length, -1 * start_utf8_length));
      }

      char *_base_value = NULL;
      if(start && end && start < end)
        _base_value = g_strndup(start, end - start);
      g_free(base_value);
      base_value = _base_value;
    }
    else if(*substitution == '#')
    {
      /*
        $(var#Pattern)
          Remove from $var the shortest part of $Pattern that matches the front end of $var.
      */

      substitution++;
      if(*substitution && g_str_has_prefix(base_value, substitution))
      {
        char *_base_value = g_strdup(base_value + strlen(substitution));
        g_free(base_value);
        base_value = _base_value;
      }
    }
    else if(*substitution == '%')
    {
      /*
        $(var%Pattern)
          Remove from $var the shortest part of $Pattern that matches the back end of $var.
      */

      substitution++;
      if(*substitution && g_str_has_suffix(base_value, substitution))
        base_value[base_value_length - strlen(substitution)] = '\0';
    }
    else if(*substitution == '/')
    {
      /*
        replacement. the following cases are possible:

        $(var/Pattern/Replacement)
          First match of Pattern, within var replaced with Replacement.
          If Replacement is omitted, then the first match of Pattern is replaced by nothing, that is, deleted.

        $(var//Pattern/Replacement)
          Global replacement. All matches of Pattern, within var replaced with Replacement.
          As above, if Replacement is omitted, then all occurrences of Pattern are replaced by nothing, that is, deleted.

        $(var/#Pattern/Replacement)
          If prefix of var matches Pattern, then substitute Replacement for Pattern.

        $(var/%Pattern/Replacement)
          If suffix of var matches Pattern, then substitute Replacement for Pattern.
      */

      const char mode = substitution[1];
      substitution++;

      char *pattern, *replacement;
      if(mode == '/' || mode == '#' || mode == '%') substitution++;
      pattern = substitution;
      while(*substitution && *substitution != '/') substitution++;
      *substitution = '\0';
      substitution++;
      replacement = substitution;
      const size_t pattern_length = strlen(pattern);
      const size_t replacement_length = strlen(replacement);

      switch(mode)
      {
        case '/':
        {
          char *_base_value = dt_util_str_replace(base_value, pattern, replacement);
          g_free(base_value);
          base_value = _base_value;
          break;
        }
        case '#':
        {
          if(g_str_has_prefix(base_value, pattern))
          {
            char *_base_value = g_malloc(base_value_length - pattern_length + replacement_length + 1);
            char *end = g_stpcpy(_base_value, replacement);
            g_stpcpy(end, base_value + pattern_length);
            g_free(base_value);
            base_value = _base_value;
          }
          break;
        }
        case '%':
        {
          if(g_str_has_suffix(base_value, pattern))
          {
            char *_base_value = g_malloc(base_value_length - pattern_length + replacement_length + 1);
            base_value[base_value_length - pattern_length] = '\0';
            char *end = g_stpcpy(_base_value, base_value);
            g_stpcpy(end, replacement);
            g_free(base_value);
            base_value = _base_value;
          }
          break;
        }
        default:
        {
          gchar *found = g_strstr_len(base_value, -1, pattern);
          if(found)
          {
            *found = '\0';
            char *_base_value = g_malloc(base_value_length - pattern_length + replacement_length + 1);
            char *end = g_stpcpy(_base_value, base_value);
            end = g_stpcpy(end, replacement);
            g_stpcpy(end, found + pattern_length);
            g_free(base_value);
            base_value = _base_value;
          }
          break;
        }
      }
    }
    else if(*substitution == '^' || *substitution == ',')
    {
      /*
        changing the case:

        $(parameter^pattern)
        $(parameter^^pattern)
        $(parameter,pattern)
        $(parameter,,pattern)
          This expansion modifies the case of alphabetic characters in parameter.
          The ‘^’ operator converts lowercase letters to uppercase;
          the ‘,’ operator converts uppercase letters to lowercase.
          The ‘^^’ and ‘,,’ expansions convert each character in the expanded value;
          the ‘^’ and ‘,’ expansions convert only the first character in the expanded value.
      */

      const char direction = substitution[0];
      const char mode = substitution[1];
      char *_base_value = NULL;
      if(direction == '^' && mode == '^')
        _base_value = g_utf8_strup (base_value, -1);
      else if(direction == ',' && mode == ',')
        _base_value = g_utf8_strdown(base_value, -1);
      else
      {
        gunichar changed = g_utf8_get_char(base_value);
        changed = direction == '^' ? g_unichar_toupper(changed) : g_unichar_tolower(changed);
        int utf8_length = g_unichar_to_utf8(changed, NULL);
        char *next = g_utf8_next_char(base_value);
        _base_value = g_malloc0(base_value_length - (next - base_value) + utf8_length + 1);
        g_unichar_to_utf8(changed, _base_value);
        g_stpcpy(_base_value + utf8_length, next);
      }
      g_free(base_value);
      base_value = _base_value;
    }
  }

  g_free(variable);
  return base_value;
}


void dt_variables_params_init(dt_variables_params_t **params)
{
  *params = g_malloc0(sizeof(dt_variables_params_t));
  (*params)->data = g_malloc0(sizeof(dt_variables_data_t));
  time_t now = time(NULL);
  localtime_r(&now, &(*params)->data->time);
  (*params)->data->exif_time = 0;
  (*params)->sequence = -1;
}

void dt_variables_params_destroy(dt_variables_params_t *params)
{
  g_free(params->data->result);
  g_free(params->data);
  g_free(params);
}

void dt_variables_set_time(dt_variables_params_t *params, time_t time)
{
  localtime_r(&time, &params->data->time);
}

void dt_variables_set_exif_time(dt_variables_params_t *params, time_t exif_time)
{
  params->data->exif_time = exif_time;
}

gchar *dt_variables_get_result(dt_variables_params_t *params)
{
  return g_strdup(params->data->result);
}

void dt_variables_expand(dt_variables_params_t *params, gchar *source, gboolean iterate)
{
  if(iterate) params->data->sequence++;

  // gather some data that might be used for variable expansion
  params->data->homedir = dt_loc_get_home_dir(NULL);

  if(g_get_user_special_dir(G_USER_DIRECTORY_PICTURES) == NULL)
    params->data->pictures_folder = g_build_path(G_DIR_SEPARATOR_S, params->data->homedir, "Pictures", (char *)NULL);
  else
    params->data->pictures_folder = g_strdup(g_get_user_special_dir(G_USER_DIRECTORY_PICTURES));

  if(params->filename)
  {
    params->data->file_ext = (g_strrstr(params->filename, ".") + 1);
    if(params->data->file_ext == (gchar *)1) params->data->file_ext = params->filename + strlen(params->filename);
  }
  else
    params->data->file_ext = NULL;

  /* image exif time */
  params->data->have_exif_tm = FALSE;
  params->data->exif_iso = 100;
  params->data->camera_maker = NULL;
  params->data->camera_alias = NULL;
  params->data->version = 0;
  params->data->stars = 0;
  if(params->imgid)
  {
    const dt_image_t *img = dt_image_cache_get(darktable.image_cache, params->imgid, 'r');
    if(sscanf(img->exif_datetime_taken, "%d:%d:%d %d:%d:%d", &params->data->exif_tm.tm_year, &params->data->exif_tm.tm_mon,
      &params->data->exif_tm.tm_mday, &params->data->exif_tm.tm_hour, &params->data->exif_tm.tm_min, &params->data->exif_tm.tm_sec) == 6)
    {
      params->data->exif_tm.tm_year -= 1900;
      params->data->exif_tm.tm_mon--;
      params->data->have_exif_tm = TRUE;
    }
    params->data->exif_iso = img->exif_iso;
    params->data->camera_maker = g_strdup(img->camera_maker);
    params->data->camera_alias = g_strdup(img->camera_alias);
    params->data->version = img->version;
    params->data->stars = (img->flags & 0x7);
    if(params->data->stars == 6) params->data->stars = -1;

    dt_image_cache_read_release(darktable.image_cache, img);
  }
  else if (params->data->exif_time) {
    localtime_r(&params->data->exif_time, &params->data->exif_tm);
    params->data->have_exif_tm = TRUE;
  }

  // go through the source and look for variables, replacing one by one
  char *iter = source;
  char *result = NULL;
  size_t result_length = 0;
  char *variable_start, *variable_end;

  while((variable_start = g_strstr_len(iter, -1, "$(")) && (variable_end = g_strstr_len(variable_start, -1, ")")))
  {
    const size_t copy_over_length = variable_start - iter;

    const size_t variable_length = variable_end - variable_start + 1;
    char *replacement = variable_get_value(params, variable_start, variable_length);
    const size_t replacement_length = replacement ? strlen(replacement) : 0;

    const size_t new_result_length = result_length + copy_over_length + replacement_length;
    if(new_result_length > result_length)
    {
      result = g_realloc(result, new_result_length + 1);
      if(copy_over_length > 0)
        memcpy(result + result_length, iter, copy_over_length);
      if(replacement_length > 0)
        memcpy(result + result_length + copy_over_length, replacement, replacement_length);
      result[new_result_length] = '\0';
      result_length = new_result_length;
    }
    iter = variable_end + 1;
    g_free(replacement);
  }

  // take care of whatever is coming past the last variable
  if(*iter)
  {
    const size_t copy_over_length = strlen(iter);
    const size_t new_result_length = result_length + copy_over_length;
    result = g_realloc(result, new_result_length + 1);
    memcpy(result + result_length, iter, copy_over_length);
  }

  g_free(params->data->result);
  params->data->result = result;

  g_free(params->data->homedir);
  g_free(params->data->pictures_folder);
  g_free(params->data->camera_maker);
  g_free(params->data->camera_alias);
}

void dt_variables_reset_sequence(dt_variables_params_t *params)
{
  params->data->sequence = 0;
}

// modelines: These editor modelines have been set for all relevant files by tools/update_modelines.sh
// vim: shiftwidth=2 expandtab tabstop=2 cindent
// kate: tab-indents: off; indent-width 2; replace-tabs on; indent-mode cstyle; remove-trailing-spaces modified;
