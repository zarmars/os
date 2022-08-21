/*file: m1_pstree.cc
 *author: zarmars
 *date: 20220813
 *desc: A simplified pstree implementation."
*/

#include <iostream>
#include <fstream>
#include <cassert>
#include <cstdint>
#include <type_traits>
#include <vector>
#include <unordered_map>
#include <sstream>
#include <string>
#include <stdexcept>

// for strcmp
#include <string.h>
#include <sys/types.h>
#include <dirent.h>

namespace os {
namespace m1 {
// Define compatible integral type for both systems of 32 bits and 64 bits.
using os_int = std::conditional_t<sizeof(void*) == 8, std::int64_t,
      std::int32_t>;


struct TreeNode {
  std::string name;
  os_int pid;
  os_int tgid;
  os_int ppid;
  bool is_thread;
  bool is_root;
  std::vector<TreeNode*> child_nodes;

  TreeNode(const std::string& _name, os_int _pid, os_int _tgid, os_int _ppid) {
    name = _name;
    pid = _pid;
    tgid = _tgid;
    ppid = _ppid;
    is_thread = (tgid != pid);
    is_root = (pid == 1);
  }

  ~TreeNode() {
    for (auto node : child_nodes) {
      assert(node != nullptr);
      delete node;
    }
  }

  bool InsertChild(TreeNode* node) {
    if (node == nullptr) {
      return false;
    }
    child_nodes.push_back(node);
    return true;
  }

  bool IsRoot() const { return is_root; }

  std::string DebugString(bool show_pids) {
    std::stringstream debug;
    if (is_thread) {
      debug << "{" << name << "}";
    } else {
      debug << name;
    }
    if (show_pids) {
      debug << "(" << pid << ")";
    }
    return debug.str();
  }
};

class PsTree {
 public:
  PsTree() = default;

  ~PsTree() {
    if (root_ != nullptr) {
      delete root_;
    }
  }

  void CreateTreeNode(std::string& name, os_int pid, os_int tgid, os_int ppid) {
    TreeNode* node = new TreeNode(name, pid, tgid, ppid);
    if (node->IsRoot()) {
      root_ = node;
    }
    all_tree_nodes_.push_back(node);
  }

  TreeNode* RootNode() const { return root_; }

  bool CreateTreeNode(std::string& status_file) {
    std::ifstream proc_status(status_file);
    if (!proc_status.is_open()) {
      std::cout << "Couldn't open file: " << status_file << std::endl;
      return false;
    }
    std::string name;
    os_int pid{-1};
    os_int tgid{-1};
    os_int ppid{-1};

    std::string line;
    while(std::getline(proc_status, line, '\n')) {
      std::string::size_type comma_pos = line.find(":");
      if (comma_pos == std::string::npos) {
        continue;
      }
      std::string attr_str = line.substr(0, comma_pos);
      std::string::size_type value_pos =
          line.find_first_not_of(" ", comma_pos + 1);
      std::string value_str = line.substr(value_pos);
      if (attr_str == "Name") {
        name = value_str;
        continue;
      }
      if (attr_str == "Tgid") {
        tgid = static_cast<os_int>(std::stoi(value_str));
        continue;
      }
      if (attr_str == "Pid") {
        pid = static_cast<os_int>(std::stoi(value_str));
        continue;
      }
      if (attr_str == "PPid") {
        ppid = static_cast<os_int>(std::stoi(value_str));
        continue;
      }
      if (!name.empty() && pid > 0 && tgid > 0 && ppid > 0) {
        break;
      }
    }
    CreateTreeNode(name, pid, tgid, ppid);
    return true;
  }

  bool CreateTreeNodes(const std::vector<std::string>& proc_files) {
    for (auto proc_file : proc_files) {
      if (!CreateTreeNode(proc_file)) {
        std::cout << "Unable to create TreeNode for: " << proc_file << std::endl;
        return false;
      }
    }
    return true;
  }

  bool BuildTreeNodeMap() {
    if (all_tree_nodes_.empty()) {
      std::cout << "Empty tree node list." << std::endl;
      return false;
    }

    for (TreeNode* node : all_tree_nodes_) {
      assert(node != nullptr);
      nodes_map_[node->pid] = node;
    }
    return true;
  }

  void BuildTree(const std::vector<std::string>& proc_files) {
    if (proc_files.empty()) {
      std::cout << "Empty process files." << std::endl;
      return;
    }
    CreateTreeNodes(proc_files);
    BuildTreeNodeMap();
    for (TreeNode* node : all_tree_nodes_) {
      if (node->IsRoot()) {
        continue;
      }
      if (nodes_map_.find(node->ppid) == nodes_map_.end()) {
        std::cout << "Unable to find parent node for: "
            << node->name << std::endl;
        return;
      }
      TreeNode* parent_node = nodes_map_[node->ppid];
      parent_node->InsertChild(node);
    }
  }

  void SortTree() {
    // sort child nodes by their pids.
  }

  // pre-order traverse of pstree.
  void PrintTree(bool show_pids) const {
    std::vector<int> edges;
    PrintTree(root_, edges, show_pids);
  }

  void PrintTree(
    TreeNode* node, std::vector<int>& edges, bool show_pids) const {
    if (node == nullptr) {
      return;
    }
    // print node branch edge if necessary
    int index = 0;
    for (int edge : edges) {
      for(; index < edge; index++) {
        std::cout << " ";
      }
      std::cout << "|";
      index++;
    }
    if (!edges.empty()) {
      std::cout << "___";
    }
    std::string node_str = node->DebugString(show_pids);
    std::cout << node_str;
    index += node_str.length();
    if (!node->child_nodes.empty()) {
      std::cout << "---";
      index += 3;
      edges.push_back(index);
      std::cout << "---";
    }
    for (int cid = 0; cid < node->child_nodes.size(); cid++) {
      PrintTree(node->child_nodes[cid], edges, show_pids);
      if (cid == 0) {
        std::cout << std::endl;
      }
    }
  }

 private:
  TreeNode* root_ = nullptr;
  std::vector<TreeNode*> all_tree_nodes_;
  std::unordered_map<os_int, TreeNode*> nodes_map_;
};

void PrintVersion() {
  std::cout << "my_pstree v1.0." << std::endl;
}

bool IsProcDir(std::string& dir_name) {
  std::size_t pos{};
  try {
    const int i{std::stoi(dir_name, &pos)};
  } catch(std::invalid_argument const& ex) {
    std::cout << "dir_name: " << dir_name << " is not a process directory.\n";
    std::cout << "std::invalid_argument::what(): " << ex.what() << std::endl;
  } catch (std::out_of_range const& ex) {
    const long long ll{std::stoi(dir_name, &pos)};
    std::cout << "std::out_of_range::what(): " << ex.what() << std::endl;
  }

  return pos == dir_name.length();
}

void ReadProcs(const std::string &dir_fpath,
    const std::string &file_name, std::vector<std::string> *files) {
  DIR *dirp = opendir(dir_fpath.c_str());
  struct dirent *dp;
  while ((dp = readdir(dirp)) != nullptr) {
    std::string dir_name(dp->d_name);
    if (!IsProcDir(dir_name)) {
      continue;
    }
    std::string file_fpath;
    if (dir_fpath.back() == '/') {
      file_fpath = dir_fpath + dir_name + std::string("/") + file_name;
    } else {
      file_fpath = dir_fpath + std::string("/") + dir_name + std::string("/") +
          file_name;
    }
    files->push_back(file_fpath);
  }
  closedir(dirp);
}

void RunPstree(bool show_pids, bool numeric_sort = false) {
  std::string dir_fpath("/proc");
  std::string status_file("status");
  std::vector<std::string> files;
  ReadProcs(dir_fpath, status_file, &files);
  PsTree pstree;
  pstree.BuildTree(files);
  if (numeric_sort) {
    pstree.SortTree();
  }
  pstree.PrintTree(show_pids);
}

} // namespace m1
} // namespace os


int main(int argc, char *argv[]) {
  bool show_pids = false;
  bool numeric_sort = false;
  bool version = false;
  for (int i = 1; i < argc; i++) {
    assert(argv[i]);
    // currently, multiple option combinations are not handled, eg. -np.
    if (strcmp(argv[i], "-p") == 0) {
      show_pids = true;
      continue;
    }
    if (strcmp(argv[i], "-n") == 0) {
      numeric_sort = true;
      continue;
    }
    if (strcmp(argv[i], "-V") == 0) {
      version = true;
      continue;
    }
  }
  std::cout << std::boolalpha << "show_pids: " << show_pids << std::endl;
  std::cout << std::boolalpha << "numeric_sort: " << numeric_sort << std::endl;
  std::cout << std::boolalpha << "version: " << version << std::endl;
  if (version) {
    os::m1::PrintVersion();
  }
  if (show_pids || numeric_sort) {
    os::m1::RunPstree(show_pids, numeric_sort);
  }
  assert(!argv[argc]);
  return 0;
}
