#include "fxp.h"

/*
 * ----------------
 *
 *  Private functions
 *
 * ----------------
 */

static struct transfer_result *transfer_recursive(struct site_info *src,
		struct site_info *dst, const char *dirname, const char *src_dir,
		const char *dst_dir) {
	struct transfer_result *ret_val = NULL;
	char *new_spath = NULL;
	char *new_dpath = NULL;
	struct file_item *first_file = NULL;

	//copy args for safety
	char *_dirname = strdup(dirname);
	char *_src_dir = strdup(src_dir);
	char *_dst_dir = strdup(dst_dir);

	struct file_item *file = filesystem_find_file(src->cur_dirlist, _dirname);

	if(file == NULL) {
		ret_val = transfer_result_create(false, strdup(_dirname), 0, 0.0f,
				false, FILE_TYPE_DIR);
		goto _transfer_recursive_cleanup;
	}

	if(file->skip) {
		log_ui_w("%s: skip\n", _dirname);
		ret_val = transfer_result_create(true, strdup(_dirname), 0, 0.0f, true,
				FILE_TYPE_DIR);
		goto _transfer_recursive_cleanup;
	}

	new_spath = path_append_dir(_src_dir, _dirname);
	new_dpath = path_append_dir(_dst_dir, _dirname);

	struct file_item *rfile = filesystem_find_file(dst->cur_dirlist, _dirname);

	if(rfile == NULL) {
		if(!ftp_mkd(dst, new_dpath)) {
			log_ui_e("%s: mkdir failed!\n", _dirname);
			ret_val = transfer_result_create(false, strdup(_dirname), 0, 0.0f,
					false, FILE_TYPE_DIR);
			goto _transfer_recursive_cleanup;
		}
	} else if(rfile->file_type != FILE_TYPE_DIR) {
		log_ui_e("%s: remote file not a directory!\n", _dirname);
		ret_val = transfer_result_create(false, strdup(_dirname), 0, 0.0f,
				false, FILE_TYPE_DIR);
		goto _transfer_recursive_cleanup;
	}

	//cwd and list src
	if(!ftp_cwd(src, _dirname)) {
		ret_val = transfer_result_create(false, strdup(_dirname), 0, 0.0f,
				false, FILE_TYPE_DIR);
		goto _transfer_recursive_cleanup;
	}

	if(!ftp_ls(src)) {
		ret_val = transfer_result_create(false, strdup(_dirname), 0, 0.0f,
				false, FILE_TYPE_DIR);
		goto _transfer_recursive_cleanup;
	}

	//cwd and list dst
	if(!ftp_cwd(dst, _dirname)) {
		ret_val = transfer_result_create(false, strdup(_dirname), 0, 0.0f,
				false, FILE_TYPE_DIR);
		goto _transfer_recursive_cleanup;
	}

	if(!ftp_ls(dst)) {
		ret_val = transfer_result_create(false, strdup(_dirname), 0, 0.0f,
				false, FILE_TYPE_DIR);
		goto _transfer_recursive_cleanup;
	}

	struct file_item *fl = src->cur_dirlist;

	if(fl == NULL) {
		log_ui_w("%s: empty dir\n", _dirname);
		ret_val = transfer_result_create(true, strdup(_dirname), 0, 0.0f, true,
				FILE_TYPE_DIR);
		goto _transfer_recursive_cleanup;
	}

	ret_val = transfer_result_create(true, strdup(_dirname), 0, 0.0f, false,
			FILE_TYPE_DIR);
	struct transfer_result *rp = ret_val;

	//count files in list
	uint32_t tot_files = 0;
	uint32_t cur_file_num = 0;

	while(fl != NULL) {
		if(fl->file_type == FILE_TYPE_FILE) {
			tot_files++;
		}
		fl = fl->next;
	}

	//make a copy to prevent cwd clear
	first_file = filesystem_file_item_cpy(src->cur_dirlist);
	fl = first_file;

	while(fl != NULL) {
		//move to end of ret list
		while(rp->next != NULL) {
			rp = rp->next;
		}

		if(fl->file_type == FILE_TYPE_FILE) {
			cur_file_num++;
			log_ui_i("%s: fxp'ing file [%d/%d]\n", fl->file_name,
					cur_file_num, tot_files);

			struct transfer_result *f_ret = fxp(src, dst, fl->file_name,
					new_spath, new_dpath);

			if(!f_ret->success) {
				log_ui_e("%s: fxp failed!\n", fl->file_name);
			}

			rp->next = f_ret;
		} else if(fl->file_type == FILE_TYPE_DIR) {
			log_ui_i("%s: fxp'ing dir..\n", fl->file_name);

			rp->next = transfer_recursive(src, dst, fl->file_name, new_spath,
					new_dpath);

			//restore current path
			if(!ftp_cwd(src, new_spath)) {
				//mark dir as failed
				ret_val->success = false;
				break;
			}

			if(!ftp_ls(src)) {
				ret_val->success = false;
				break;
			}

			//restore current path
			if(!ftp_cwd(dst, new_dpath)) {
				//mark dir as failed
				ret_val->success = false;
				break;
			}

			if(!ftp_ls(dst)) {
				ret_val->success = false;
				break;
			}
		}

		fl = fl->next;
	}

_transfer_recursive_cleanup:
	filesystem_file_item_destroy(first_file);

	free(new_spath);
	free(new_dpath);
	free(_dirname);
	free(_src_dir);
	free(_dst_dir);
	return ret_val;
}

/*
 * ----------------
 *
 * Public functions
 *
 * ----------------
 */

struct transfer_result *fxp(struct site_info *src, struct site_info *dst,
		const char *filename, const char *src_dir, const char *dst_dir) {
	printf("from %s to %s with file: %s\n", src_dir, dst_dir, filename);
	struct transfer_result *ret_val = NULL;
	char *new_dpath = NULL;
	char *new_spath = NULL;
	char *s_port = NULL;

	struct file_item *file = filesystem_find_file(src->cur_dirlist, filename);
	struct timeval time_start;
	struct timeval time_end;

	bool use_sscn = src->enable_sscn && dst->enable_sscn;

	if(file == NULL) {
		ret_val = transfer_result_create(false, strdup(filename), 0, 0.0f,
				false, FILE_TYPE_FILE);
		goto _fxp_cleanup;
	}

	if(file->skip) {
		log_ui_w("%s: skip\n", filename);
		ret_val = transfer_result_create(true, strdup(filename), 0, 0.0f, true,
				FILE_TYPE_FILE);
		goto _fxp_cleanup;
	}

	new_dpath = path_append_file(dst_dir, filename);

	if(dst->xdupe_enabled) {
		if(site_xdupe_has(dst, new_dpath)) {
			log_ui_w("%s: skip (x-dupe)\n", filename);
			ret_val = transfer_result_create(true, strdup(filename), 0, 0.0f,
					true, FILE_TYPE_FILE);
			goto _fxp_cleanup;
		}
	}


	if(use_sscn) {
		if(!ftp_sscn(src, !src->enforce_sscn_server_mode)) {
			ret_val = transfer_result_create(false, strdup(filename), 0, 0.0f,
					false, FILE_TYPE_FILE);
			goto _fxp_cleanup;
		}

		if(!ftp_sscn(dst, src->enforce_sscn_server_mode)) {
			ret_val = transfer_result_create(false, strdup(filename), 0, 0.0f,
					false, FILE_TYPE_FILE);
			goto _fxp_cleanup;
		}
	}

	struct pasv_details *pv = ftp_pasv(src, src->use_tls && !use_sscn);

	if(pv == NULL) {
		ret_val = transfer_result_create(false, strdup(filename), 0, 0.0f,
				false, FILE_TYPE_FILE);
		goto _fxp_cleanup;
	}

	uint32_t s_len = strlen(pv->unparsed) + 9;
	s_port = malloc(s_len);
	snprintf(s_port, s_len, "PORT %s\r\n", pv->unparsed);

	control_send(dst, s_port);
	if(control_recv(dst) != 200) {
		ret_val = transfer_result_create(false, strdup(filename), 0, 0.0f,
				false, FILE_TYPE_FILE);
		goto _fxp_cleanup;
	}

	new_spath = path_append_file(src_dir, filename);

	if(!ftp_stor(dst, new_dpath)) {
		ret_val = transfer_result_create(false, strdup(filename), 0, 0.0f,
				false, FILE_TYPE_FILE);
		goto _fxp_cleanup;
	}

	//also mark dst as busy
	site_busy(dst);

	gettimeofday(&time_start, NULL);

	if(!ftp_retr(src, new_spath)) {
		//if retr fails, cancel the STOR with pasv trick
		control_send(src, "PASV\r\n");
		control_recv(src);

		//wait for STOR resp
		control_recv(dst);
		ret_val = transfer_result_create(false, strdup(filename), 0, 0.0f,
				false, FILE_TYPE_FILE);
		goto _fxp_cleanup;
	}

	if(control_recv(src) != 226) {
		ret_val = transfer_result_create(false, strdup(filename), 0, 0.0f,
				false, FILE_TYPE_FILE);
		goto _fxp_cleanup;
	}

	if(control_recv(dst) != 226) {
		ret_val = transfer_result_create(false, strdup(filename), 0, 0.0f,
				false, FILE_TYPE_FILE);
		goto _fxp_cleanup;
	}

	gettimeofday(&time_end, NULL);
	char *speed = s_calc_transfer_speed(&time_start, &time_end, file->size);
	char *s_size = parse_file_size(file->size);

	log_ui_i("%s: fxp'd %s at %s in %ds\n", filename, s_size, speed,
			time_end.tv_sec - time_start.tv_sec);

	free(speed);
	free(s_size);
	ret_val = transfer_result_create(true, strdup(filename), file->size,
			calc_transfer_speed(&time_start, &time_end, file->size), false,
			FILE_TYPE_FILE);

_fxp_cleanup:
	free(new_spath);
	free(new_dpath);
	free(s_port);

	return ret_val;
}

struct transfer_result *fxp_recursive(struct site_info *src,
		struct site_info *dst, const char *dirname, const char *src_dir,
		const char *dst_dir) {
	struct timeval time_start;
	struct timeval time_end;

	gettimeofday(&time_start, NULL);

	char *src_origin = strdup(src_dir);
	char *dst_origin = strdup(dst_dir);
	struct transfer_result *ret = transfer_recursive(src, dst, dirname,
			src_dir, dst_dir);

	gettimeofday(&time_end, NULL);

	ftp_cwd(src, src_origin);
	ftp_cwd(dst, dst_origin);
	ftp_ls(src);
	ftp_ls(dst);

	log_ui_i("Stats: %s\n", s_gen_stats(ret,
			time_end.tv_sec - time_start.tv_sec));

	free(src_origin);
	free(dst_origin);

	return ret;
}
