void bcast_mesh(Mesh& mesh, CommPtr new_comm, bool is_source) {
  CommPtr old_comm = mesh.comm();
  if (new_comm->rank() == 0) {
    CHECK(is_source);
  }
  I32 dim;
  if (is_source)
    dim = mesh.dim();
  new_comm->bcast(dim);
  if (!is_source) {
    mesh.set_dim(dim);
    mesh.set_verts(0);
  }
  for (Int d = 0; d <= dim; ++d) {
    if (d > VERT && !is_source)
      mesh.set_ents(d, Adj(LOs({}), Read<I8>({})));
    I32 ntags;
    if (is_source)
      ntags = mesh.ntags(d);
    new_comm->bcast(ntags);
    for (Int i = 0; i < ntags; ++i) {
      TagBase const* tag = nullptr;
      if (is_source)
        tag = mesh.get_tag(d, i);
      std::string name;
      if (is_source)
        name = tag->name();
      new_comm->bcast_string(name);
      I32 ncomps;
      if (is_source)
        ncomps = tag->ncomps();
      new_comm->bcast(ncomps);
      I32 tag_type;
      if (is_source)
        tag_type = tag->type();
      new_comm->bcast(tag_type);
      if (!is_source) {
        switch (tag_type) {
          case OSH_I8: mesh.add_tag<I8>(d, name, ncomps); break;
          case OSH_I32: mesh.add_tag<I32>(d, name, ncomps); break;
          case OSH_I64: mesh.add_tag<I64>(d, name, ncomps); break;
          case OSH_F64: mesh.add_tag<Real>(d, name, ncomps); break;
        }
      }
    }
    if (!is_source && mesh.has_tag(d, "owner"))
      mesh.set_own_idxs(d, LOs({}));
  }
}