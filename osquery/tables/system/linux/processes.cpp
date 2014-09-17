// Copyright 2004-present Facebook. All Rights Reserved.

#include <string>
#include <fstream>
#include <streambuf>
#include <sstream>

#include <stdlib.h>
#include <unistd.h>
#include <proc/readproc.h>

#include <boost/lexical_cast.hpp>

#include "osquery/core.h"
#include "osquery/database.h"
#include "osquery/filesystem.h"

using namespace osquery::db;

namespace osquery {
namespace tables {

#define PROC_SELECTS                                                 \
  PROC_FILLCOM | PROC_EDITCMDLCVT | PROC_FILLMEM | PROC_FILLSTATUS | \
      PROC_FILLSTAT

std::string proc_name(const proc_t* proc_info) {
  char cmd[17]; // cmd is a 16 char buffer

  memset(cmd, 0, 17);
  memcpy(cmd, proc_info->cmd, 16);
  return std::string(cmd);
}

std::string proc_attr(const std::string& attr, const proc_t* proc_info) {
  std::stringstream filename;

  filename << "/proc/" << proc_info->tid << "/" << attr;
  return filename.str();
}

std::string proc_cmdline(const proc_t* proc_info) {
  std::string attr;
  std::string result;

  attr = proc_attr("cmdline", proc_info);
  std::ifstream fd(attr, std::ios::in | std::ios::binary);
  if (fd) {
    result = std::string(std::istreambuf_iterator<char>(fd),
                         std::istreambuf_iterator<char>());
  }

  return result;
}

std::string proc_link(const proc_t* proc_info) {
  std::string attr;
  std::string result;
  char* link_path;
  long path_max;
  int bytes;

  // The exe is a symlink to the binary on-disk.
  attr = proc_attr("exe", proc_info);
  path_max = pathconf(attr.c_str(), _PC_PATH_MAX);
  link_path = (char*)malloc(path_max);

  memset(link_path, 0, path_max);
  bytes = readlink(attr.c_str(), link_path, path_max);
  if (bytes >= 0) {
    result = std::string(link_path);
  }

  free(link_path);
  return result;
}

QueryData genProcesses() {
  QueryData results;

  proc_t* proc_info;
  PROCTAB* proc = openproc(PROC_SELECTS);

  // Populate proc struc for each process.
  while (proc_info = readproc(proc, NULL)) {
    Row r;

    r["pid"] = boost::lexical_cast<std::string>(proc_info->tid);
    r["name"] = proc_name(proc_info);
    r["cmdline"] = proc_cmdline(proc_info);
    r["path"] = proc_link(proc_info);
    r["on_disk"] = osquery::pathExists(r["path"]).toString();

    r["resident_size"] = boost::lexical_cast<std::string>(proc_info->vm_rss);
    r["phys_footprint"] = boost::lexical_cast<std::string>(proc_info->vm_size);
    r["user_time"] = boost::lexical_cast<std::string>(proc_info->utime);
    r["system_time"] = boost::lexical_cast<std::string>(proc_info->stime);
    r["start_time"] = boost::lexical_cast<std::string>(proc_info->start_time);
    r["parent"] = boost::lexical_cast<std::string>(proc_info->ppid);

    results.push_back(r);
    freeproc(proc_info);
  }

  closeproc(proc);

  return results;
}
}
}
