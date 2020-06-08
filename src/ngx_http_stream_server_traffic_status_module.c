
/*
 * Copyright (C) YoungJoo Kim (vozlt)
 */


#include <ngx_config.h>

#include "ngx_http_stream_server_traffic_status_module.h"
#include "ngx_http_stream_server_traffic_status_display.h"
#include "ngx_http_stream_server_traffic_status_dump.h"

static void
ngx_http_stream_server_traffic_status_rbtree_insert_value(ngx_rbtree_node_t *temp,
                                                  ngx_rbtree_node_t *node,
                                                  ngx_rbtree_node_t *sentinel);
static ngx_int_t
ngx_http_stream_server_traffic_status_init_zone(ngx_shm_zone_t *shm_zone, void *data);

static char *ngx_http_stream_server_traffic_status_zone(ngx_conf_t *cf,
    ngx_command_t *cmd, void *conf);
static char *ngx_http_stream_server_traffic_status_dump(ngx_conf_t *cf,
                                                ngx_command_t *cmd, void *conf);
static char *ngx_http_stream_server_traffic_status_average_method(ngx_conf_t *cf,
    ngx_command_t *cmd, void *conf);

static void *ngx_http_stream_server_traffic_status_create_main_conf(ngx_conf_t *cf);
static char *ngx_http_stream_server_traffic_status_init_main_conf(ngx_conf_t *cf,
    void *conf);
static void *ngx_http_stream_server_traffic_status_create_loc_conf(ngx_conf_t *cf);
static char *ngx_http_stream_server_traffic_status_merge_loc_conf(ngx_conf_t *cf,
    void *parent, void *child);
static ngx_int_t ngx_http_stream_server_traffic_status_init_worker(ngx_cycle_t *cycle);
static void ngx_http_stream_server_traffic_status_exit_worker(ngx_cycle_t *cycle);

static ngx_conf_enum_t  ngx_http_stream_server_traffic_status_display_format[] = {
    { ngx_string("json"), NGX_HTTP_STREAM_SERVER_TRAFFIC_STATUS_FORMAT_JSON },
    { ngx_string("html"), NGX_HTTP_STREAM_SERVER_TRAFFIC_STATUS_FORMAT_HTML },
    { ngx_string("jsonp"), NGX_HTTP_STREAM_SERVER_TRAFFIC_STATUS_FORMAT_JSONP },
    { ngx_string("prometheus"), NGX_HTTP_STREAM_SERVER_TRAFFIC_STATUS_FORMAT_PROMETHEUS },
    { ngx_null_string, 0 }
};


static ngx_conf_enum_t  ngx_http_stream_server_traffic_status_average_method_post[] = {
    { ngx_string("AMM"), NGX_HTTP_STREAM_SERVER_TRAFFIC_STATUS_AVERAGE_METHOD_AMM },
    { ngx_string("WMA"), NGX_HTTP_STREAM_SERVER_TRAFFIC_STATUS_AVERAGE_METHOD_WMA },
    { ngx_null_string, 0 }
};


static ngx_command_t ngx_http_stream_server_traffic_status_commands[] = {

    { ngx_string("stream_server_traffic_status"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_stream_server_traffic_status_loc_conf_t, enable),
      NULL },

    { ngx_string("stream_server_traffic_status_zone"),
      NGX_HTTP_MAIN_CONF|NGX_CONF_NOARGS|NGX_CONF_TAKE1,
      ngx_http_stream_server_traffic_status_zone,
      0,
      0,
      NULL },

    {ngx_string("stream_server_traffic_status_dump"),
     NGX_HTTP_MAIN_CONF | NGX_CONF_TAKE12, ngx_http_stream_server_traffic_status_dump,
     0, 0, NULL},
      
    { ngx_string("stream_server_traffic_status_display"),
      NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_NOARGS|NGX_CONF_TAKE1,
      ngx_http_stream_server_traffic_status_display,
      0,
      0,
      NULL },

    { ngx_string("stream_server_traffic_status_display_format"),
      NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_enum_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_stream_server_traffic_status_loc_conf_t, format),
      &ngx_http_stream_server_traffic_status_display_format },

    { ngx_string("stream_server_traffic_status_display_jsonp"),
      NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_str_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_stream_server_traffic_status_loc_conf_t, jsonp),
      NULL },

    { ngx_string("stream_server_traffic_status_average_method"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE12,
      ngx_http_stream_server_traffic_status_average_method,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

    ngx_null_command
};


static ngx_http_module_t ngx_http_stream_server_traffic_status_module_ctx = {
    NULL,                                                    /* preconfiguration */
    NULL,                                                    /* postconfiguration */

    ngx_http_stream_server_traffic_status_create_main_conf,  /* create main configuration */
    ngx_http_stream_server_traffic_status_init_main_conf,    /* init main configuration */

    NULL,                                                    /* create server configuration */
    NULL,                                                    /* merge server configuration */

    ngx_http_stream_server_traffic_status_create_loc_conf,   /* create location configuration */
    ngx_http_stream_server_traffic_status_merge_loc_conf,    /* merge location configuration */
};


ngx_module_t ngx_http_stream_server_traffic_status_module = {
    NGX_MODULE_V1,
    &ngx_http_stream_server_traffic_status_module_ctx,       /* module context */
    ngx_http_stream_server_traffic_status_commands,          /* module directives */
    NGX_HTTP_MODULE,                                         /* module type */
    NULL,                                                    /* init master */
    NULL,                                                    /* init module */
      NULL,                                                    /* init process */
      //ngx_http_stream_server_traffic_status_init_worker, /* init process */
    NULL,                                                    /* init thread */
    NULL,                                                    /* exit thread */
    NULL,                                                    /* exit process */
    //ngx_http_stream_server_traffic_status_exit_worker, /* exit process */
    NULL,                                                    /* exit master */
    NGX_MODULE_V1_PADDING
};

static char *ngx_http_stream_server_traffic_status_dump(ngx_conf_t *cf,
                                                ngx_command_t *cmd,
                                                void *conf) {
  ngx_http_stream_server_traffic_status_ctx_t *ctx = conf;

  ngx_int_t rc;
  ngx_str_t *value;

  value = cf->args->elts;

  ctx->dump = 1;

  ctx->dump_file = value[1];
      ngx_conf_log_error(NGX_LOG_INFO, cf, 0, "dump_file: \"%s\"",
                         &value[1]);
  /* second argument process */
  if (cf->args->nelts == 3) {
    rc = ngx_parse_time(&value[2], 0);
    if (rc == NGX_ERROR) {
      ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "invalid parameter \"%V\"",
                         &value[2]);
      goto invalid;
    }
    ctx->dump_period = (ngx_msec_t)rc;
  }

  return NGX_CONF_OK;

invalid:

  return NGX_CONF_ERROR;
}

static void
ngx_http_stream_server_traffic_status_rbtree_insert_value(ngx_rbtree_node_t *temp,
                                                  ngx_rbtree_node_t *node,
                                                  ngx_rbtree_node_t *sentinel) {
  ngx_rbtree_node_t **p;
  ngx_http_stream_server_traffic_status_node_t *vtsn, *vtsnt;

  for (;;) {

    if (node->key < temp->key) {

      p = &temp->left;

    } else if (node->key > temp->key) {

      p = &temp->right;

    } else { /* node->key == temp->key */

      vtsn = (ngx_http_stream_server_traffic_status_node_t *)&node->color;
      vtsnt = (ngx_http_stream_server_traffic_status_node_t *)&temp->color;

      p = (ngx_memn2cmp(vtsn->data, vtsnt->data, vtsn->len, vtsnt->len) < 0)
              ? &temp->left
              : &temp->right;
    }

    if (*p == sentinel) {
      break;
    }

    temp = *p;
  }

  *p = node;
  node->parent = temp;
  node->left = sentinel;
  node->right = sentinel;
  ngx_rbt_red(node);
}


static ngx_int_t
ngx_http_stream_server_traffic_status_init_zone1(ngx_shm_zone_t *shm_zone, void *data) {
  ngx_http_stream_server_traffic_status_ctx_t *octx = data;

  size_t len;
  ngx_slab_pool_t *shpool;
  ngx_rbtree_node_t *sentinel;
  ngx_http_stream_server_traffic_status_ctx_t *ctx;

  ctx = shm_zone->data;

  if (octx) {
    ctx->rbtree = octx->rbtree;
    return NGX_OK;
  }

  shpool = (ngx_slab_pool_t *)shm_zone->shm.addr;

  if (shm_zone->shm.exists) {
    ctx->rbtree = shpool->data;
    return NGX_OK;
  }

  ctx->rbtree = ngx_slab_alloc(shpool, sizeof(ngx_rbtree_t));
  if (ctx->rbtree == NULL) {
    return NGX_ERROR;
  }

  shpool->data = ctx->rbtree;

  sentinel = ngx_slab_alloc(shpool, sizeof(ngx_rbtree_node_t));
  if (sentinel == NULL) {
    return NGX_ERROR;
  }

  ngx_rbtree_init(ctx->rbtree, sentinel,
                  ngx_http_stream_server_traffic_status_rbtree_insert_value);

  len = sizeof(" in stream_server_traffic_status_zone \"\"") + shm_zone->shm.name.len;

  shpool->log_ctx = ngx_slab_alloc(shpool, len);
  if (shpool->log_ctx == NULL) {
    return NGX_ERROR;
  }

  ngx_sprintf(shpool->log_ctx, " in stream_server_traffic_status_zone \"%V\"%Z",
              &shm_zone->shm.name);

  return NGX_OK;
}


static char *ngx_http_stream_server_traffic_status_zone(ngx_conf_t *cf,
                                                ngx_command_t *cmd,
                                                void *conf) {


  ngx_str_t *value, name;
  ngx_http_stream_server_traffic_status_ctx_t *ctx;
  ngx_uint_t i;
  



  value = cf->args->elts;

  ctx = ngx_http_conf_get_module_main_conf(
      cf, ngx_http_stream_server_traffic_status_module);
  if (ctx == NULL) {
    return NGX_CONF_ERROR;
  }

  ctx->enable = 1;

  ngx_str_set(&name, NGX_HTTP_STREAM_SERVER_TRAFFIC_STATUS_DEFAULT_SHM_NAME);

  ssize_t size;
  u_char *p;    
  ngx_shm_zone_t *shm_zone;
  ngx_str_t s;
  
  size = NGX_HTTP_STREAM_SERVER_TRAFFIC_STATUS_DEFAULT_SHM_SIZE;

  for (i = 1; i < cf->args->nelts; i++) {
        ngx_conf_log_error(NGX_LOG_INFO, cf, 0, " parameter \"%V\"",
                       &value[i]);
    if (ngx_strncmp(value[i].data, "shared:", 7) == 0) {

      
      name.data = value[i].data + 7;

      p = (u_char *)ngx_strchr(name.data, ':');
      if (p == NULL) {
	//        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "invalid shared size \"%V\"",
        ngx_conf_log_error(NGX_LOG_INFO, cf, 0, "invalid shared size \"%V\"",
                           &value[i]);
        return NGX_CONF_ERROR;
      }

      name.len = p - name.data;

      s.data = p + 1;
      s.len = value[i].data + value[i].len - s.data;

      size = ngx_parse_size(&s);
      if (size == NGX_ERROR) {
	//        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "invalid shared size \"%V\"",
        ngx_conf_log_error(NGX_LOG_INFO, cf, 0, "invalid shared size \"%V\"",
                           &value[i]);
        return NGX_CONF_ERROR;
      }

      if (size < (ssize_t)(8 * ngx_pagesize)) {
	//        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "shared \"%V\" is too small",
        ngx_conf_log_error(NGX_LOG_INFO, cf, 0, "shared \"%V\" is too small",
                           &value[i]);
        return NGX_CONF_ERROR;
      }

      continue;
    }

    ngx_conf_log_error(NGX_LOG_INFO, cf, 0, "invalid parameter \"%V\"",
		       //    ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "invalid parameter \"%V\"",
                       &value[i]);
    return NGX_CONF_ERROR;
  }

  shm_zone = ngx_shared_memory_add(cf, &name, size,
                                   &ngx_http_stream_server_traffic_status_module);
  if (shm_zone == NULL) {
    return NGX_CONF_ERROR;
  }

  if (shm_zone->data) {
    ctx = shm_zone->data;

    //    ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
    ngx_conf_log_error(NGX_LOG_INFO, cf, 0,
                       "stream_server_traffic_status: \"%V\" is already bound to key",
                       &name);

    return NGX_CONF_ERROR;
  }

  ctx->shm_zone = shm_zone;
  ctx->shm_name = name;
  ctx->shm_size = size;
  shm_zone->init = ngx_http_stream_server_traffic_status_init_zone;
  shm_zone->data = ctx;

  return NGX_CONF_OK;
}



static char *
ngx_http_stream_server_traffic_status_zone(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_str_t                                    *value, name;
    ngx_uint_t                                    i;
    ngx_http_stream_server_traffic_status_ctx_t  *ctx;

    value = cf->args->elts;

    ctx = ngx_http_conf_get_module_main_conf(cf, ngx_http_stream_server_traffic_status_module);
    if (ctx == NULL) {
        return NGX_CONF_ERROR;
    }

    ctx->enable = 1;

    ngx_str_set(&name, NGX_HTTP_STREAM_SERVER_TRAFFIC_STATUS_DEFAULT_SHM_NAME);

    for (i = 1; i < cf->args->nelts; i++) {
        if (ngx_strncmp(value[i].data, "shared:", 7) == 0) {
            name.data = value[i].data + 7;
            name.len = value[i].len - 7;
            continue;
        }

        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "invalid parameter \"%V\"", &value[i]);
        return NGX_CONF_ERROR;
    }

    ctx->shm_name = name;

    return NGX_CONF_OK;
}


static char *
ngx_http_stream_server_traffic_status_average_method(ngx_conf_t *cf,
    ngx_command_t *cmd, void *conf)
{
    ngx_http_stream_server_traffic_status_loc_conf_t *stscf = conf;

    char       *rv;
    ngx_int_t   rc;
    ngx_str_t  *value;

    value = cf->args->elts;

    cmd->offset = offsetof(ngx_http_stream_server_traffic_status_loc_conf_t, average_method);
    cmd->post = &ngx_http_stream_server_traffic_status_average_method_post;

    rv = ngx_conf_set_enum_slot(cf, cmd, conf);
    if (rv != NGX_CONF_OK) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "invalid parameter \"%V\"", &value[1]);
        goto invalid;
    }

    /* second argument process */
    if (cf->args->nelts == 3) {
        rc = ngx_parse_time(&value[2], 0);
        if (rc == NGX_ERROR) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "invalid parameter \"%V\"", &value[2]);
            goto invalid;
        }
        stscf->average_period = (ngx_msec_t) rc;
    }

    return NGX_CONF_OK;

invalid:

    return NGX_CONF_ERROR;
}


static void *
ngx_http_stream_server_traffic_status_create_main_conf(ngx_conf_t *cf)
{
    ngx_http_stream_server_traffic_status_ctx_t  *ctx;

    ctx = ngx_pcalloc(cf->pool, sizeof(ngx_http_stream_server_traffic_status_ctx_t));
    if (ctx == NULL) {
        return NULL;
    }

    ctx->enable = NGX_CONF_UNSET;
    ctx->dump = NGX_CONF_UNSET;
    ctx->dump_period = NGX_CONF_UNSET_MSEC;

    return ctx;
}


static char *
ngx_http_stream_server_traffic_status_init_main_conf(ngx_conf_t *cf, void *conf)
{
    ngx_http_stream_server_traffic_status_ctx_t  *ctx = conf;

    //    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, cf->log, 0,
    ngx_log_error(NGX_LOG_INFO, cf->log, 0,
                   "http stream sts init main conf");

    ngx_conf_init_value(ctx->enable, 0);
  ngx_conf_init_value(ctx->dump, 0);
  ngx_conf_merge_msec_value(ctx->dump_period, ctx->dump_period,
                            NGX_HTTP_STREAM_SERVER_TRAFFIC_STATUS_DEFAULT_DUMP_PERIOD *
                                1000);
    return NGX_CONF_OK;
}


static void *
ngx_http_stream_server_traffic_status_create_loc_conf(ngx_conf_t *cf)
{
    ngx_http_stream_server_traffic_status_loc_conf_t  *conf;

    conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_stream_server_traffic_status_loc_conf_t));
    if (conf == NULL) {
        return NULL;
    }

    /*
     * set by ngx_pcalloc():
     *
     *     conf->shm_zone = { NULL, ... };
     *     conf->enable = 0;
     *     conf->shm_name = { 0, NULL };
     *     conf->stats = { 0, ... };
     *     conf->start_msec = 0;
     *     conf->format = 0;
     *     conf->jsonp = { 1, NULL };
     *     conf->average_method = 0;
     *     conf->average_period = 0;
     */

    conf->start_msec = ngx_http_stream_server_traffic_status_current_msec();
    conf->enable = NGX_CONF_UNSET;
    conf->shm_zone = NGX_CONF_UNSET_PTR;
    conf->format = NGX_CONF_UNSET;
    conf->average_method = NGX_CONF_UNSET;
    conf->average_period = NGX_CONF_UNSET_MSEC;

    conf->node_caches = ngx_pcalloc(cf->pool, sizeof(ngx_rbtree_node_t *)
                                    * (NGX_HTTP_STREAM_SERVER_TRAFFIC_STATUS_UPSTREAM_FG + 1));
    conf->node_caches[NGX_HTTP_STREAM_SERVER_TRAFFIC_STATUS_UPSTREAM_NO] = NULL;
    conf->node_caches[NGX_HTTP_STREAM_SERVER_TRAFFIC_STATUS_UPSTREAM_UA] = NULL;
    conf->node_caches[NGX_HTTP_STREAM_SERVER_TRAFFIC_STATUS_UPSTREAM_UG] = NULL;
    conf->node_caches[NGX_HTTP_STREAM_SERVER_TRAFFIC_STATUS_UPSTREAM_FG] = NULL;

    return conf;
}


static char *
ngx_http_stream_server_traffic_status_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_http_stream_server_traffic_status_loc_conf_t *prev = parent;
    ngx_http_stream_server_traffic_status_loc_conf_t *conf = child;

    ngx_http_stream_server_traffic_status_ctx_t  *ctx;

    //    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, cf->log, 0,
    ngx_log_error(NGX_LOG_INFO, cf->log, 0,
                   "http stream sts merge loc conf");

    ctx = ngx_http_conf_get_module_main_conf(cf, ngx_http_stream_server_traffic_status_module);

    if (!ctx->enable) {
        return NGX_CONF_OK;
    }

    ngx_conf_merge_value(conf->enable, prev->enable, 1);
    ngx_conf_merge_ptr_value(conf->shm_zone, prev->shm_zone, NULL);
    ngx_conf_merge_value(conf->format, prev->format,
                         NGX_HTTP_STREAM_SERVER_TRAFFIC_STATUS_FORMAT_JSON);
    ngx_conf_merge_str_value(conf->jsonp, prev->jsonp,
                             NGX_HTTP_STREAM_SERVER_TRAFFIC_STATUS_DEFAULT_JSONP);
    ngx_conf_merge_value(conf->average_method, prev->average_method,
                         NGX_HTTP_STREAM_SERVER_TRAFFIC_STATUS_AVERAGE_METHOD_AMM);
    ngx_conf_merge_msec_value(conf->average_period, prev->average_period,
                              NGX_HTTP_STREAM_SERVER_TRAFFIC_STATUS_DEFAULT_AVG_PERIOD * 1000);

       conf->shm_name = ctx->shm_name;
  /* ngx_str_t name; */
  /* ngx_shm_zone_t *shm_zone; */
  /*     name = ctx->shm_name; */

  /* shm_zone = ngx_shared_memory_add(cf, &name, 0, */
  /*                                  &ngx_http_stream_server_traffic_status_module); */
  /* if (shm_zone == NULL) { */
  /*   return NGX_CONF_ERROR; */
  /* } */

  /* conf->shm_zone = shm_zone; */
  /* conf->shm_name = name; */

    return NGX_CONF_OK;
}



ngx_msec_t
ngx_http_stream_server_traffic_status_current_msec(void)
{
    time_t           sec;
    ngx_uint_t       msec;
    struct timeval   tv;

    ngx_gettimeofday(&tv);

    sec = tv.tv_sec;
    msec = tv.tv_usec / 1000;

    return (ngx_msec_t) sec * 1000 + msec;
}

static ngx_int_t ngx_http_stream_server_traffic_status_init_worker(ngx_cycle_t *cycle) {
  ngx_event_t *dump_event;
  ngx_http_stream_server_traffic_status_ctx_t *ctx;
  ngx_log_error(NGX_LOG_INFO, cycle->log, 0, "http sts init worker");
  //  ngx_log_debug0(NGX_LOG_DEBUG_HTTP, cycle->log, 0, "http sts init2 worker");

  ctx = ngx_http_cycle_get_module_main_conf(
      cycle, ngx_http_stream_server_traffic_status_module);

  if (ctx == NULL) {
    //    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, cycle->log, 0,
    ngx_log_error(NGX_LOG_INFO, cycle->log, 0,
                   "sts::init_worker(): is bypassed due to no http block in configure file");
    return NGX_OK;
  }

  if (!(ctx->enable & ctx->dump) || ctx->rbtree == NULL) {
    //ngx_log_debug0(NGX_LOG_DEBUG_HTTP, cycle->log, 0,
    ngx_log_error(NGX_LOG_INFO, cycle->log, 0,
                   "sts::init_worker(): is bypassed");
    return NGX_OK;
  }

  /* dumper */
  /* dump_event = &ctx->dump_event; */
  /* dump_event->handler = ngx_http_stream_server_traffic_status_dump_handler; */
  /* dump_event->log = ngx_cycle->log; */
  /* dump_event->data = ctx; */
  /* ngx_add_timer(dump_event, 1000); */

  /* restore */
  //ngx_http_stream_server_traffic_status_dump_restore(dump_event);

  return NGX_OK;
}

static void ngx_http_stream_server_traffic_status_exit_worker(ngx_cycle_t *cycle) {
  ngx_event_t *dump_event;
  ngx_http_stream_server_traffic_status_ctx_t *ctx;

  ngx_log_error(NGX_LOG_INFO, cycle->log, 0, "http sts exit worker");
  //  ngx_log_debug0(NGX_LOG_DEBUG_HTTP, cycle->log, 0, "http sts exit2 worker");

  ctx = ngx_http_cycle_get_module_main_conf(
      cycle, ngx_http_stream_server_traffic_status_module);

  if (ctx == NULL) {
    //    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, cycle->log, 0,
		   ngx_log_error(NGX_LOG_INFO, cycle->log, 0,
                   "sts::exit_worker(): is bypassed due to no http block in configure file");
    return;
  }

  if (!(ctx->enable & ctx->dump) || ctx->rbtree == NULL) {
    //    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, cycle->log, 0,
		   ngx_log_error(NGX_LOG_INFO, cycle->log, 0,
                   "sts::exit_worker(): is bypassed");
    return;
  }

  
  /* dump */
  dump_event = &ctx->dump_event;
  dump_event->log = ngx_cycle->log;
  dump_event->data = ctx;
  ngx_http_stream_server_traffic_status_dump_execute(dump_event);
}


/* vi:set ft=c ts=4 sw=4 et fdm=marker: */
