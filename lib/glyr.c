/***********************************************************
* This file is part of glyr
* + a commnadline tool and library to download various sort of musicrelated metadata.
* + Copyright (C) [2011]  [Christopher Pahl]
* + Hosted at: https://github.com/sahib/glyr
*
* glyr is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* glyr is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with glyr. If not, see <http://www.gnu.org/licenses/>.
**************************************************************/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <getopt.h>

#include "stringlib.h"
#include "core.h"

#include "glyr.h"

#include "cover.h"
#include "lyrics.h"
#include "photos.h"
#include "ainfo.h"
#include "similiar.h"
#include "review.h"
#include "tracklist.h"
#include "tags.h"
#include "albumlist.h"
#include "relations.h"

#include "config.h"

/*--------------------------------------------------------*/

const char * err_strings[] =
{
    "all okay",
    "bad option",
    "bad value for option",
    "NULL pointer for struct",
    "No provider specified",
    "Unknown ID for getter",
    "Ignored cache",
    "Stopped by callback",
    NULL
};

/*--------------------------------------------------------*/

// prototypes
static int glyr_parse_from(const char * arg, GlyQuery * settings);
static int glyr_set_info(GlyQuery * s, int at, const char * arg);
static void glyr_register_group(GlyPlugin * providers, enum GLYR_GROUPS GIDmask, bool value);

/*--------------------------------------------------------*/

// return a descriptive string on error ID
const char * Gly_strerror(enum GLYR_ERROR ID)
{
    if(ID < (sizeof(err_strings)/sizeof(const char *)))
    {
        return err_strings[ID];
    }
    return NULL;
}

// Fill yours in here if you added a new one.
GlyPlugin getwd_commands [] =
{
    {"cover" ,   "c",  NULL, false, {NULL, NULL, NULL, false}, GET_COVER     },
    {"lyrics",   "l",  NULL, false, {NULL, NULL, NULL, false}, GET_LYRIC     },
    {"photos",   "p",  NULL, false, {NULL, NULL, NULL, false}, GET_PHOTO     },
    {"ainfo",    "a",  NULL, false, {NULL, NULL, NULL, false}, GET_AINFO     },
    {"similiar", "s",  NULL, false, {NULL, NULL, NULL, false}, GET_SIMILIAR  },
    {"review",   "r",  NULL, false, {NULL, NULL, NULL, false}, GET_REVIEW    },
    {"albumlist","i",  NULL, false, {NULL, NULL, NULL, false}, GET_ALBUMLIST },
    {"tags",     "t",  NULL, false, {NULL, NULL, NULL, false}, GET_TAGS      },
    {"relations","n",  NULL, false, {NULL, NULL, NULL, false}, GET_RELATIONS },
    {"tracklist","r",  NULL, false, {NULL, NULL, NULL, false}, GET_TRACKLIST },
    {NULL,       NULL, NULL, 42,    {NULL, NULL, NULL, false}, GRP_NONE      }
};

/*-----------------------------------------------*/

const char * Gly_version(void)
{
    return "Version "glyr_VERSION_MAJOR"."glyr_VERSION_MINOR" ("glyr_VERSION_NAME") of ["__DATE__"] compiled at ["__TIME__"]";
}

/*-----------------------------------------------*/

GlyMemCache * Gly_copy_cache(GlyMemCache * source)
{
    return DL_copy(source);
}

/*-----------------------------------------------*/
// _opt_
/*-----------------------------------------------*/

// Seperate method because va_arg struggles with function pointers
//int GlyOpt_dlcallback(GlyQuery * settings, int (*dl_cb)(GlyMemCache *, GlyQuery *), void * userp)
int GlyOpt_dlcallback(GlyQuery * settings, DL_callback dl_cb, void * userp)
{
    if(settings)
    {
        settings->callback.download     = dl_cb;
        settings->callback.user_pointer = userp;
        return GLYRE_OK;
    }
    return GLYRE_EMPTY_STRUCT;
}

/*-----------------------------------------------*/

int GlyOpt_type(GlyQuery * s, enum GLYR_GET_TYPE type)
{
    if(s == NULL) return GLYRE_EMPTY_STRUCT;
    if(type < GET_UNSURE)
    {
        s->type = type;
        return GLYRE_OK;
    }
    return GLYRE_BAD_VALUE;
}

/*-----------------------------------------------*/

int GlyOpt_artist(GlyQuery * s, char * artist)
{
    if(s == NULL) return GLYRE_EMPTY_STRUCT;
    s->artist = artist;
    return GLYRE_OK;
}

/*-----------------------------------------------*/

int GlyOpt_album(GlyQuery * s, char * album)
{
    if(s == NULL) return GLYRE_EMPTY_STRUCT;
    s->album = album;
    return GLYRE_OK;
}

/*-----------------------------------------------*/

int GlyOpt_title(GlyQuery * s, char * title)
{
    if(s == NULL) return GLYRE_EMPTY_STRUCT;
    s->title = title;
    return GLYRE_OK;
}

/*-----------------------------------------------*/

static int size_set(int * ref, int size)
{
    if(size < -1 && ref)
    {
        *ref = -1;
        return GLYRE_BAD_VALUE;
    }

    if(ref)
    {
        *ref = size;
        return GLYRE_OK;
    }
    return GLYRE_BAD_OPTION;
}

/*-----------------------------------------------*/

int GlyOpt_cmaxsize(GlyQuery * s, int size)
{
    if(s == NULL) return GLYRE_EMPTY_STRUCT;
    return size_set(&s->cover.max_size,size);
}

/*-----------------------------------------------*/

int GlyOpt_cminsize(GlyQuery * s, int size)
{
    if(s == NULL) return GLYRE_EMPTY_STRUCT;
    return size_set(&s->cover.min_size,size);
}

/*-----------------------------------------------*/

int GlyOpt_parallel(GlyQuery * s, unsigned long val)
{
    if(s == NULL) return GLYRE_EMPTY_STRUCT;
    s->parallel = (long)val;
    return GLYRE_OK;
}

/*-----------------------------------------------*/

int GlyOpt_timeout(GlyQuery * s, unsigned long val)
{
    if(s == NULL) return GLYRE_EMPTY_STRUCT;
    s->timeout = (long)val;
    return GLYRE_OK;
}

/*-----------------------------------------------*/

int GlyOpt_redirects(GlyQuery * s, unsigned long val)
{
    if(s == NULL) return GLYRE_EMPTY_STRUCT;
    s->redirects = (long)val;
    return GLYRE_OK;
}

/*-----------------------------------------------*/

int GlyOpt_lang(GlyQuery * s, char * langcode)
{
    if(s == NULL) return GLYRE_EMPTY_STRUCT;
    if(langcode != NULL)
    {
        s->lang = langcode;
        return GLYRE_OK;
    }
    return GLYRE_BAD_VALUE;
}

/*-----------------------------------------------*/

int GlyOpt_number(GlyQuery * s, unsigned int num)
{
    if(s == NULL) return GLYRE_EMPTY_STRUCT;
    s->number = num;
    return GLYRE_OK;

}

/*-----------------------------------------------*/

int GlyOpt_verbosity(GlyQuery * s, unsigned int level)
{
    if(s == NULL) return GLYRE_EMPTY_STRUCT;
    s->verbosity = level;
    return GLYRE_OK;
}

/*-----------------------------------------------*/

int GlyOpt_from(GlyQuery * s, const char * from)
{
    if(s == NULL) return GLYRE_EMPTY_STRUCT;
    return glyr_parse_from(from,s);
}

/*-----------------------------------------------*/

int GlyOpt_color(GlyQuery * s, bool iLikeColorInMyLife)
{
    if(s == NULL) return GLYRE_EMPTY_STRUCT;
    s->color_output = iLikeColorInMyLife;
    return GLYRE_OK;
}

/*-----------------------------------------------*/

int GlyOpt_formats(GlyQuery * s, const char * formats)
{
    if(s == NULL) return GLYRE_EMPTY_STRUCT;
    if(formats != NULL)
    {
        glyr_set_info(s,3,formats);
        return GLYRE_OK;
    }
    return GLYRE_BAD_VALUE;
}

/*-----------------------------------------------*/

int GlyOpt_plugmax(GlyQuery * s, int plugmax)
{
    if(s == NULL) return GLYRE_EMPTY_STRUCT;
    if(plugmax < 0)
    {
        return GLYRE_BAD_VALUE;
    }

    s->plugmax = plugmax;
    return GLYRE_OK;
}

/*-----------------------------------------------*/

int GlyOpt_duplcheck(GlyQuery * s, bool duplcheck)
{
    if(s == NULL) return GLYRE_EMPTY_STRUCT;
    s->duplcheck = duplcheck;
    return GLYRE_OK;
}

/*-----------------------------------------------*/

int GlyOpt_fuzzyness(GlyQuery * s, int fuzz)
{
    if(s == NULL) return GLYRE_EMPTY_STRUCT;
    s->fuzzyness = fuzz;
    return GLYRE_OK;
}

/*-----------------------------------------------*/

int GlyOpt_groupedDL(GlyQuery * s, bool groupedDL)
{
    if(s == NULL) return GLYRE_EMPTY_STRUCT;
    s->groupedDL = groupedDL;
    return GLYRE_OK;
}

/*-----------------------------------------------*/

int GlyOpt_download(GlyQuery * s, bool download)
{
    if(s == NULL) return GLYRE_EMPTY_STRUCT;
    s->download = download;
    return GLYRE_OK;
}

/*-----------------------------------------------*/

int GlyOpt_call_direct_provider(GlyQuery * s, const char * provider)
{
    if(s == NULL) return GLYRE_EMPTY_STRUCT;
    return glyr_set_info(s,4,provider);
}

/*-----------------------------------------------*/

int GlyOpt_call_direct_url(GlyQuery * s, const char * URL)
{
    if(s == NULL) return GLYRE_EMPTY_STRUCT;
    return glyr_set_info(s,5,URL);
}

/*-----------------------------------------------*/

int GlyOpt_call_direct_use(GlyQuery * s, bool use)
{
    if(s == NULL) return GLYRE_EMPTY_STRUCT;
    s->call_direct.use = true;
    return GLYRE_OK;
}

/*-----------------------------------------------*/

GlyMemCache * Gly_clist_at(GlyCacheList * clist, int iter)
{
    if(clist && iter >= 0)
    {
        return clist->list[iter];
    }
    return NULL;
}

/*-----------------------------------------------*/

const char * Gly_groupname_by_id(enum GLYR_GROUPS ID)
{
    return grp_id_to_name(ID);
}

/*-----------------------------------------------*/
/*-----------------------------------------------*/
/*-----------------------------------------------*/

static void set_query_on_defaults(GlyQuery * glyrs)
{
    glyrs->type = GET_UNSURE;
    glyrs->artist = NULL;
    glyrs->album  = NULL;
    glyrs->title  = NULL;
    glyrs->providers = NULL;
    glyrs->cover.min_size = DEFAULT_CMINSIZE;
    glyrs->cover.max_size = DEFAULT_CMAXSIZE;
    glyrs->number = DEFAULT_NUMBER;
    glyrs->parallel  = DEFAULT_PARALLEL;
    glyrs->redirects = DEFAULT_REDIRECTS;
    glyrs->timeout   = DEFAULT_TIMEOUT;
    glyrs->verbosity = DEFAULT_VERBOSITY;
    glyrs->lang = DEFAULT_LANG;
    glyrs->plugmax = DEFAULT_PLUGMAX;
    glyrs->color_output = PRT_COLOR;
    glyrs->download = DEFAULT_DOWNLOAD;
    glyrs->groupedDL = DEFAULT_GROUPEDL;
    glyrs->callback.download = NULL;
    glyrs->callback.user_pointer = NULL;
    glyrs->itemctr = 0;
    glyrs->fuzzyness = DEFAULT_FUZZYNESS;
    glyrs->duplcheck = DEFAULT_DUPLCHECK;
    glyrs->formats = DEFAULT_FORMATS;

    // direct call
    glyrs->call_direct.url = NULL;
    glyrs->call_direct.use = DEFAULT_CALL_DIRECT_USE;
    glyrs->call_direct.provider = DEFAULT_CALL_DIRECT_PROVIDER;
    memset(glyrs->info,0,sizeof(const char * ) * 10);
}

/*-----------------------------------------------*/

void Gly_init_query(GlyQuery * glyrs)
{
    set_query_on_defaults(glyrs);
}

/*-----------------------------------------------*/

void Gly_destroy_query(GlyQuery * sets)
{
    if(sets)
    {
        size_t i = 0;
        for(; i < 10; i++)
        {
            if(sets->info[i])
            {
                free((char*)sets->info[i]);
                sets->info[i] = NULL;
            }
        }

        if(sets->providers != NULL)
        {
            free(sets->providers);
            sets->providers = NULL;
        }

        set_query_on_defaults(sets);
    }
}

/*-----------------------------------------------*/

GlyMemCache * Gly_download(const char * url, GlyQuery * s)
{
    return download_single(url,s,NULL);
}

/*-----------------------------------------------*/

void Gly_free_list(GlyCacheList * lst)
{
    DL_free_lst(lst);
}

/*-----------------------------------------------*/

void Gly_free_cache(GlyMemCache * c)
{
    DL_free(c);
}

/*-----------------------------------------------*/

GlyMemCache * Gly_new_cache(void)
{
    return DL_init();
}

/*-----------------------------------------------*/

void Gly_add_to_list(GlyCacheList * l, GlyMemCache * c)
{
    DL_add_to_list(l,c);
}

/*-----------------------------------------------*/

static GlyCacheList * call_parser_direct(GlyQuery * s)
{
    GlyCacheList * re = NULL;
    GlyPlugin * cPlug = Gly_get_provider_by_id(s->type);
    if(cPlug != NULL)
    {
        size_t iter;
        for(iter = 0; cPlug[iter].name; iter++)
        {
            if(!strcmp(cPlug[iter].key,s->call_direct.provider) || !strcmp(cPlug[iter].name,s->call_direct.provider))
            {
                GlyMemCache * c = NULL;
                const char  * dl_url = (s->call_direct.url) ? cPlug[iter].plug.url_callback(s) : s->call_direct.url;
                if(dl_url != NULL)
                {
                    c = download_single(dl_url,s,NULL);
                    if(c != NULL)
                    {
                        cb_object capo;
                        plugin_init(&capo,NULL,NULL,s,NULL,NULL,0);
                        capo.cache = c;

                        re = cPlug[iter].plug.parser_callback(&capo);

                        DL_free(c);
                        c = NULL;
                        capo.cache = NULL;
                    }
                }
                break;
            }
        }
        free(cPlug);
        cPlug = NULL;
    }
    return re;
}

/*-----------------------------------------------*/

GlyCacheList * Gly_get(GlyQuery * settings, enum GLYR_ERROR * e)
{
    if(e) *e = GLYRE_OK;
    if(!settings)
    {
        if(e) *e = GLYRE_EMPTY_STRUCT;
        return NULL;
    }

    if(settings->call_direct.use)
    {
        return call_parser_direct(settings);
    }

    if(!settings->providers)
    {
        GlyPlugin * p = Gly_get_provider_by_id(settings->type);
        if(p != NULL)
        {
            glyr_register_group(p,GRP_ALL,true);
            settings->providers = p;
        }
        else
        {
            if(e) *e = GLYRE_NO_PROVIDER;
            return NULL;
        }
    }

    GlyCacheList * result = NULL;
    switch(settings->type)
    {
    case GET_COVER:
        result = get_cover(settings);
        break;
    case GET_LYRIC:
        result = get_lyrics(settings);
        break;
    case GET_PHOTO:
        result = get_photos(settings);
        break;
    case GET_AINFO:
        result = get_ainfo(settings);
        break;
    case GET_SIMILIAR:
        result = get_similiar(settings);
        break;
    case GET_REVIEW:
        result = get_review(settings);
        break;
    case GET_TAGS:
        result = get_tags(settings);
        break;
    case GET_TRACKLIST:
        result =  get_tracklist(settings);
        break;
    case GET_ALBUMLIST:
        result = get_albumlist(settings);
        break;
    case GET_RELATIONS:
        result = get_relations(settings);
        break;
    default:
        if(e) *e = GLYRE_UNKNOWN_GET;
    }

    settings->itemctr = 0;
    if(result != NULL && !result->size)
    {
	Gly_free_list(result);
	result = NULL;	
    }
    return result;
}

/*-----------------------------------------------*/

void Gly_init(void)
{
    if(curl_global_init(CURL_GLOBAL_ALL))
    {
        glyr_message(-1,NULL,stderr,"?? libcurl failed to init.\n");
    }
}

/*-----------------------------------------------*/

void Gly_cleanup(void)
{
    curl_global_cleanup();
}

/*-----------------------------------------------*/

int Gly_write(GlyQuery * s, GlyMemCache * data, const char * path)
{
    int bytes = -1;
    if(path)
    {
        if(!strcasecmp(path,"null"))
        {
            bytes = 0;
        }
        else if(!strcasecmp(path,"stdout"))
        {
            bytes=fwrite(data->data,1,data->size,stdout);
            fputc('\n',stdout);
        }
        else if(!strcasecmp(path,"stderr"))
        {
            bytes=fwrite(data->data,1,data->size,stderr);
            fputc('\n',stderr);
        }
        else
        {
            FILE * fp = fopen(path,"wb" /* welcome back */);
            if(fp)
            {
                bytes=fwrite(data->data,1,data->size,fp);
                fclose(fp);
            }
            else
            {
                glyr_message(-1,NULL,stderr,"Gly_write: Unable to write to '%s'!\n",path);
            }
        }
    }
    return bytes;
}

/*-----------------------------------------------*/

GlyPlugin * Gly_get_provider_by_id(enum GLYR_GET_TYPE ID)
{
    switch(ID)
    {
    case GET_COVER:
        return glyr_get_cover_providers();
    case GET_LYRIC:
        return glyr_get_lyric_providers();
    case GET_PHOTO:
        return glyr_get_photo_providers();
    case GET_AINFO:
        return glyr_get_ainfo_providers();
    case GET_SIMILIAR:
        return glyr_get_similiar_providers();
    case GET_REVIEW:
        return glyr_get_review_providers();
    case GET_TRACKLIST:
        return glyr_get_tracklist_providers();
    case GET_TAGS:
        return glyr_get_tags_providers();
    case GET_RELATIONS:
        return glyr_get_relations_providers();
    case GET_ALBUMLIST:
        return glyr_get_albumlist_providers();
    case GET_UNSURE   :
        return copy_table(getwd_commands,sizeof(getwd_commands));
    default       :
        return NULL;
    }
}

/*-----------------------------------------------*/
/* End of from outerspace visible methods.       */
/*-----------------------------------------------*/

static int glyr_set_info(GlyQuery * s, int at, const char * arg)
{
    int result = GLYRE_OK;
    if(s && arg && at >= 0 && at < 10)
    {
        if(s->info[at] != NULL)
            free((char*)s->info[at]);

        s->info[at] = strdup(arg);
        switch(at)
        {
        case 0:
            s->artist = (char*)s->info[at];
            break;
        case 1:
            s->album = (char*)s->info[at];
            break;
        case 2:
            s->title = (char*)s->info[at];
            break;
        case 3:
            s->formats = (char*)s->info[at];
            break;
        case 4:
            s->call_direct.provider = (char*)s->info[at];
            break;
        case 5:
            s->call_direct.url = (char*)s->info[at];
            break;
        default:
            glyr_message(2,s,stderr,"Warning: wrong AT for glyr_info_at!\n");
        }
    }
    else
    {
        result = GLYRE_BAD_VALUE;
    }
    return result;
}

/*-----------------------------------------------*/

static void glyr_register_group(GlyPlugin * providers, enum GLYR_GROUPS GIDmask, bool value)
{
    int i = 0;
    if(GIDmask == GRP_ALL) /* (Un)Register ALL */
    {
        while(providers[i].name)
        {
            if(providers[i].key)
            {
                providers[i].use = value;
            }
            i++;
        }
        return;
    }
    else /* Register a specific Group */
    {
        while(providers[i].name)
        {
            if(providers[i].gid & GIDmask)
            {
                providers[i].use = value;
            }
            i++;
        }
    }
}

/*-----------------------------------------------*/

static int glyr_parse_from(const char * arg, GlyQuery * settings)
{
    int result = GLYRE_OK;
    if(settings && arg)
    {
        GlyPlugin * what_pair = Gly_get_provider_by_id(settings->type);
        if(settings->providers != NULL)
        {
            free(settings->providers);
        }
        settings->providers = what_pair;

        if(what_pair)
        {
            char * c_arg = NULL;
            size_t length = strlen(arg);
            size_t offset = 0;

            while( (c_arg = get_next_word(arg,DEFAULT_FROM_ARGUMENT_DELIM, &offset, length)) != NULL)
            {
                char * track = c_arg;
                bool value = true;
                if(*c_arg && *c_arg == '-')
                {
                    value = false;
                    c_arg++;
                }
                else if(*c_arg && *c_arg == '+')
                {
                    value = true;
                    c_arg++;
                }

                if(!strcasecmp(c_arg, grp_id_to_name(GRP_ALL)))
                {
                    glyr_register_group(what_pair,GRP_ALL,value);
                }
                else if(!strcasecmp(c_arg, grp_id_to_name(GRP_SAFE)))
                {
                    glyr_register_group(what_pair, GRP_SAFE,value);
                }
                else if(!strcasecmp(c_arg, grp_id_to_name(GRP_USFE)))
                {
                    glyr_register_group(what_pair, GRP_USFE,value);
                }
                else if(!strcasecmp(c_arg, grp_id_to_name(GRP_SPCL)))
                {
                    glyr_register_group(what_pair, GRP_SPCL,value);
                }
                else if(!strcasecmp(c_arg, grp_id_to_name(GRP_FAST)))
                {
                    glyr_register_group(what_pair,GRP_FAST,value);
                }
                else if(!strcasecmp(c_arg, grp_id_to_name(GRP_SLOW)))
                {
                    glyr_register_group(what_pair,GRP_SLOW,value);
                }
                else
                {
                    int i = 0;
                    bool found = false;
                    for(; what_pair[i].name; i++)
                    {
                        if(!strcasecmp(what_pair[i].name,c_arg) || (what_pair[i].key && !strcasecmp(what_pair[i].key,c_arg)))
                        {
                            what_pair[i].use = value;
                            found = true;
                        }
                    }
                    if(!found)
                    {
                        glyr_message(1,settings,stderr,C_R"*"C_" Unknown provider '%s'\n",c_arg);
                        result = GLYRE_BAD_VALUE;
                    }
                }

                // Give the user at least a hint.
                free(track);
            }
        }
    }
    return result;
}

/*-----------------------------------------------*/
