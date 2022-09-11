/*file: m1_pstree.cc
 *author: zarmars
 *date: 20220813 *desc: A simplified pstree implementation."
*/

#include <cassert>
#include <cstdint>
#include <ctype.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

// for strcmp
#include <string.h>
#include <sys/types.h>
#include <dirent.h>

namespace os {
namespace m1 {
// Define compatible integral type for both systems of 32 bits and 64 bits.
using os_int = std::conditional_t<sizeof(void*) == 8, std::int64_t,
      std::int32_t>;

void ReadProcs(const std::string &dir_fpath, const std::string &file_name,
               const std::string &skip, std::vector<std::string> *files);

struct TreeNode {
  std::string name;
  os_int pid;
  os_int tgid;
  os_int ppid;
  bool is_thread;
  bool has_threads;
  bool is_root;
  std::vector<TreeNode*> child_nodes;

  TreeNode(const std::string &_name, os_int _pid, os_int _tgid, os_int _ppid,
           os_int threads) {
    name = _name;
    pid = _pid;
    tgid = _tgid;
    ppid = _ppid;
    is_thread = (tgid != pid);
    has_threads = (threads > 1);
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

  std::string Name() const { return name; }

  bool IsRoot() const { return is_root; }

  bool IsThread() const { return is_thread; }

  bool HasThreads() const { return has_threads; }

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

  TreeNode *CreateTreeNode(std::string &name, os_int pid, os_int tgid,
                           os_int ppid, os_int threads) {
    TreeNode *node = new TreeNode(name, pid, tgid, ppid, threads);
    if (node->IsRoot()) {
      root_ = node;
    }
    all_tree_nodes_.push_back(node);
    return node;
  }

  TreeNode* RootNode() const { return root_; }

  TreeNode *CreateTreeNode(const std::string &status_file,
                           const std::string &proc_name) {
    std::ifstream proc_status(status_file);
    if (!proc_status.is_open()) {
      std::cout << "Couldn't open file: " << status_file << std::endl;
      return nullptr;
    }
    std::string name;
    os_int pid{-1};
    os_int tgid{-1};
    os_int ppid{-1};
    os_int threads{-1};

    std::string line;
    while(std::getline(proc_status, line, '\n')) {
      std::string::size_type comma_pos = line.find(":");
      if (comma_pos == std::string::npos) {
        continue;
      }
      std::string attr_str = line.substr(0, comma_pos);
      std::string::size_type value_pos = comma_pos + 1;
      while (value_pos < line.size() && isspace(line.at(value_pos))) {
        value_pos++;
      }
      if (value_pos == line.size()) {
        continue;
      }
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
      if (attr_str == "Threads") {
        threads = static_cast<os_int>(std::stoi(value_str));
      }
      if (!name.empty() && pid > 0 && tgid > 0 && ppid > 0 && threads > 0) {
        break;
      }
    }
    name = (proc_name.empty() ? name : proc_name);
    return CreateTreeNode(name, pid, tgid, ppid, threads);
  }

  bool CreateTreeNodes(const std::vector<std::string> &proc_files) {
    for (auto proc_file : proc_files) {
      TreeNode *node = CreateTreeNode(proc_file, "");
      if (node == nullptr) {
        std::cout << "Unable to create TreeNode for: " << proc_file
                  << std::endl;
        return false;
      } else if (!node->IsThread() && node->HasThreads()) {
        std::size_t file_pos = proc_file.rfind('/');
        assert(file_pos != std::string::npos);
        std::size_t dir_pos = proc_file.rfind('/', file_pos - 1);
        assert(dir_pos != std::string::npos);
        std::string proc_file_name =
            proc_file.substr(dir_pos + 1, file_pos - dir_pos - 1);
        // std::cout << "process proc file: " << proc_file_name << std::endl;
        std::string proc_file_dir = proc_file.substr(0, file_pos);
        std::vector<std::string> threads_file;
        const std::string threads_dir = proc_file_dir + std::string("/task");
        ReadProcs(threads_dir, "status", proc_file_name, &threads_file);
        for (auto thread_file : threads_file) {
          // std::cout << "process thread file: " << thread_file << std::endl;
          TreeNode *thread_node = CreateTreeNode(thread_file, node->Name());
          if (thread_node == nullptr) {
            std::cout << "Unable to create TreeNode for: " << thread_file
                      << std::endl;
            return false;
          }
        }
      }
    }
    // Create virtual kernal node.
    std::string virtual_root("kernal");
    CreateTreeNode(virtual_root, 0, 0, 0, 1);
    return true;
  }

  bool BuildTreeNodeMap() {
    if (all_tree_nodes_.empty()) {
      std::cout << "Empty tree node list." << std::endl;
      return false;
    }

    for (TreeNode *node : all_tree_nodes_) {
      assert(node != nullptr);
      nodes_map_[node->pid] = node;
    }
    return true;
  }

  void BuildTree(const std::vector<std::string> &proc_files) {
    if (proc_files.empty()) {
      std::cout << "Empty process files." << std::endl;
      return;
    }
    CreateTreeNodes(proc_files);
    BuildTreeNodeMap();
    for (TreeNode *node : all_tree_nodes_) {
      if (node->IsRoot()) {
        continue;
      }
      os_int parent_id = node->IsThread() ? node->tgid : node->ppid;
      if (nodes_map_.find(parent_id) == nodes_map_.end()) {
        std::cout << "Unable to find parent node for: " << node->name << "("
                  << node->pid << ")" << std::endl;
        return;
      }
      TreeNode *parent_node = nodes_map_[parent_id];
      parent_node->InsertChild(node);
    }
  }

  void SortTree() {
    // sort child nodes by their pids.
    // NOTE:
    // Because we read files from the /proc directory in order, the process tree
    // already follows the node pid ascending relationship when it is created,
    // so we don't need to sort it.
  }

  // pre-order traverse of pstree.
  void PrintTree(bool show_pids) const {
    std::cout << std::endl << std::endl;
    std::stringstream ss;
    std::vector<int> branches;
    std::vector<bool> enable_branches;
    PrintTree(root_, 0, branches, enable_branches, show_pids, ss);
    std::cout << ss.str();
  }

  void PrintTree(TreeNode *node, int start_pos, std::vector<int> &branches,
                 std::vector<bool> &enable_branches, bool show_pids,
                 std::stringstream &ss) const {
    if (node == nullptr) {
      return;
    }

    std::string node_str = node->DebugString(show_pids);
    ss << node_str;
    int branch_pos = start_pos + node_str.size() + 3;
    branches.push_back(branch_pos);
    enable_branches.push_back(true);
    for (std::size_t cid = 0; cid < node->child_nodes.size(); cid++) {
      if (cid == 0) {
        if (node->child_nodes.size() > 1) {
          ss << "--+--";
        } else {
          ss << "-----";
        }
      }
      if (cid + 1 == node->child_nodes.size()) {
        enable_branches.back() = false;
      }
      PrintTree(node->child_nodes[cid], branch_pos + 2, branches,
                enable_branches, show_pids, ss);

      if (cid + 1 < node->child_nodes.size()) {
        ss << "\n";
        if (!branches.empty()) {
          int last_br_pos = 0;
          for (int br_ind = 0; br_ind < branches.size(); br_ind++) {
            if (enable_branches[br_ind]) {
              ss << std::string(branches[br_ind] - last_br_pos - 1, ' ') << "|";
            } else {
              ss << std::string(branches[br_ind] - last_br_pos, ' ');
            }
            last_br_pos = branches[br_ind];
          }
          ss << "--";
        }
      }
    }
    branches.pop_back();
    enable_branches.pop_back();
  }

private:
  TreeNode *root_ = nullptr;
  std::vector<TreeNode *> all_tree_nodes_;
  std::unordered_map<os_int, TreeNode *> nodes_map_;
};

void PrintVersion() { std::cout << "my_pstree v1.0." << std::endl; }

bool IsProcDir(std::string& dir_name) {
  std::size_t pos{};
  try {
    const int i{std::stoi(dir_name, &pos)};
  } catch(std::invalid_argument const& ex) {
    // std::cout << "dir_name: " << dir_name << " is not a process directory.\n";
    // std::cout << "std::invalid_argument::what(): " << ex.what() << std::endl;
  } catch (std::out_of_range const& ex) {
    const long long ll{std::stoi(dir_name, &pos)};
    // std::cout << "std::out_of_range::what(): " << ex.what() << std::endl;
  }

  return pos == dir_name.length();
}

void ReadProcs(const std::string &dir_fpath, const std::string &file_name,
               const std::string &skip, std::vector<std::string> *files) {
  DIR *dirp = opendir(dir_fpath.c_str());
  struct dirent *dp;
  while ((dp = readdir(dirp)) != nullptr) {
    std::string dir_name(dp->d_name);
    if (!IsProcDir(dir_name)) {
      continue;
    }
    if (dir_name == skip) {
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
  std::string skip("");
  std::vector<std::string> files;
  // Read process directory to get corresponding status file.
  ReadProcs(dir_fpath, status_file, skip, &files);
  // Read hidden(ls -al won't show) thread directory to get corresponding status
  // file,
  PsTree pstree;
  pstree.BuildTree(files);
  if (numeric_sort) {
    pstree.SortTree();
  }
  pstree.PrintTree(show_pids);
}

} // namespace m1
} // namespace os


// Hits: Using g++ m1_pstree.cc to build file, then ./a.out -p to execute it.
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
