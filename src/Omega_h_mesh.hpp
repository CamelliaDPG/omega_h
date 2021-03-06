#ifndef OMEGA_H_MESH_HPP
#define OMEGA_H_MESH_HPP

#include <string>
#include <vector>

#include <Omega_h_adj.hpp>
#include <Omega_h_comm.hpp>
#include <Omega_h_dist.hpp>
#include <Omega_h_library.hpp>
#include <Omega_h_tag.hpp>

namespace Omega_h {

namespace inertia {
struct Rib;
}

class Mesh {
 public:
  Mesh(Library* library);
  Library* library() const;
  void set_comm(CommPtr const& comm);
  void set_family(Omega_h_Family family);
  void set_dim(Int dim_in);
  void set_verts(LO nverts_in);
  void set_ents(Int ent_dim, Adj down);
  CommPtr comm() const;
  Omega_h_Parting parting() const;
  inline Int dim() const {
    OMEGA_H_CHECK(0 <= dim_ && dim_ <= 3);
    return dim_;
  }
  inline Omega_h_Family family() const { return family_; }
  LO nents(Int ent_dim) const;
  LO nelems() const;
  LO nregions() const;
  LO nfaces() const;
  LO nedges() const;
  LO nverts() const;
  GO nglobal_ents(Int dim);
  template <typename T>
  void add_tag(Int dim, std::string const& name, Int ncomps);
  template <typename T>
  void add_tag(Int dim, std::string const& name, Int ncomps, Read<T> array,
      bool internal = false);
  template <typename T>
  void set_tag(
      Int dim, std::string const& name, Read<T> array, bool internal = false);
  TagBase const* get_tagbase(Int dim, std::string const& name) const;
  template <typename T>
  Tag<T> const* get_tag(Int dim, std::string const& name) const;
  template <typename T>
  Read<T> get_array(Int dim, std::string const& name) const;
  void remove_tag(Int dim, std::string const& name);
  bool has_tag(Int dim, std::string const& name) const;
  Int ntags(Int dim) const;
  TagBase const* get_tag(Int dim, Int i) const;
  bool has_ents(Int dim) const;
  bool has_adj(Int from, Int to) const;
  Adj get_adj(Int from, Int to) const;
  Adj ask_down(Int from, Int to);
  LOs ask_verts_of(Int dim);
  LOs ask_elem_verts();
  Adj ask_up(Int from, Int to);
  Graph ask_star(Int dim);
  Graph ask_dual();

 public:
  typedef std::shared_ptr<TagBase> TagPtr;
  typedef std::shared_ptr<Adj> AdjPtr;
  typedef std::shared_ptr<Dist> DistPtr;
  typedef std::shared_ptr<inertia::Rib> RibPtr;

 private:
  typedef std::vector<TagPtr> TagVector;
  typedef TagVector::iterator TagIter;
  typedef TagVector::const_iterator TagCIter;
  TagIter tag_iter(Int dim, std::string const& name);
  TagCIter tag_iter(Int dim, std::string const& name) const;
  void check_dim(Int dim) const;
  void check_dim2(Int dim) const;
  void add_adj(Int from, Int to, Adj adj);
  Adj derive_adj(Int from, Int to);
  Adj ask_adj(Int from, Int to);
  void react_to_set_tag(Int dim, std::string const& name);
  Omega_h_Family family_;
  Int dim_;
  CommPtr comm_;
  Int parting_;
  Int nghost_layers_;
  LO nents_[DIMS];
  TagVector tags_[DIMS];
  AdjPtr adjs_[DIMS][DIMS];
  Remotes owners_[DIMS];
  DistPtr dists_[DIMS];
  RibPtr rib_hints_;
  Library* library_;

 public:
  void add_coords(Reals array);
  Reals coords() const;
  void set_coords(Reals const& array);
  Read<GO> globals(Int dim) const;
  Reals ask_lengths();
  Reals ask_qualities();
  Reals ask_sizes();
  void set_owners(Int dim, Remotes owners);
  Remotes ask_owners(Int dim);
  Read<I8> owned(Int dim);
  Dist ask_dist(Int dim);
  Int nghost_layers() const;
  void set_parting(Omega_h_Parting parting_in, Int nlayers, bool verbose);
  void set_parting(Omega_h_Parting parting_in, bool verbose = false);
  void balance(bool predictive = false);
  Graph ask_graph(Int from, Int to);
  template <typename T>
  Read<T> sync_array(Int ent_dim, Read<T> a, Int width);
  template <typename T>
  Read<T> sync_subset_array(
      Int ent_dim, Read<T> a_data, LOs a2e, T default_val, Int width);
  template <typename T>
  Read<T> reduce_array(Int ent_dim, Read<T> a, Int width, Omega_h_Op op);
  template <typename T>
  Read<T> owned_array(Int ent_dim, Read<T> a, Int width);
  void sync_tag(Int dim, std::string const& name);
  void reduce_tag(Int dim, std::string const& name, Omega_h_Op op);
  bool operator==(Mesh& other);
  Real min_quality();
  Real max_length();
  bool could_be_shared(Int ent_dim) const;
  bool owners_have_all_upward(Int ent_dim) const;
  bool have_all_upward() const;
  Mesh copy_meta() const;
  RibPtr rib_hints() const;
  void set_rib_hints(RibPtr hints);
  Real imbalance(Int ent_dim = -1) const;
};

bool can_print(Mesh* mesh);

Real repro_sum_owned(Mesh* mesh, Int dim, Reals a);

Reals average_field(Mesh* mesh, Int dim, LOs a2e, Int ncomps, Reals v2x);
Reals average_field(Mesh* mesh, Int dim, Int ncomps, Reals v2x);

TagSet get_all_mesh_tags(Mesh* mesh);
void ask_for_mesh_tags(Mesh* mesh, TagSet const& tags);

void reorder_by_hilbert(Mesh* mesh);
void reorder_by_globals(Mesh* mesh);

#define OMEGA_H_EXPL_INST_DECL(T)                                              \
  extern template Tag<T> const* Mesh::get_tag<T>(                              \
      Int dim, std::string const& name) const;                                 \
  extern template Read<T> Mesh::get_array<T>(Int dim, std::string const& name) \
      const;                                                                   \
  extern template void Mesh::add_tag<T>(                                       \
      Int dim, std::string const& name, Int ncomps);                           \
  extern template void Mesh::add_tag<T>(Int dim, std::string const& name,      \
      Int ncomps, Read<T> array, bool internal);                               \
  extern template void Mesh::set_tag(                                          \
      Int dim, std::string const& name, Read<T> array, bool internal);         \
  extern template Read<T> Mesh::sync_array(Int ent_dim, Read<T> a, Int width); \
  extern template Read<T> Mesh::owned_array(                                   \
      Int ent_dim, Read<T> a, Int width);                                      \
  extern template Read<T> Mesh::sync_subset_array(                             \
      Int ent_dim, Read<T> a_data, LOs a2e, T default_val, Int width);         \
  extern template Read<T> Mesh::reduce_array(                                  \
      Int ent_dim, Read<T> a, Int width, Omega_h_Op op);
OMEGA_H_EXPL_INST_DECL(I8)
OMEGA_H_EXPL_INST_DECL(I32)
OMEGA_H_EXPL_INST_DECL(I64)
OMEGA_H_EXPL_INST_DECL(Real)
#undef OMEGA_H_EXPL_INST_DECL

}  // namespace Omega_h

#endif
