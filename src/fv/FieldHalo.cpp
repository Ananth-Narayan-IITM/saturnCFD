#include "fv/FieldHalo.hpp"

#include <climits>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <type_traits>

#ifdef SATURN_USE_MPI
#include <mpi.h>
#endif

namespace saturn {

#ifdef SATURN_USE_MPI
static_assert(std::is_same_v<Real, double>,
              "FieldHalo MPI staging expects Real=double");
const MPI_Datatype kMpiReal = MPI_DOUBLE;
#endif

namespace {

int checkedSize(BigIndex values, const char* where) {
  if (values < 0 || values > INT_MAX)
    throw std::runtime_error(std::string(where) +
                             ": halo message exceeds INT_MAX");
  return static_cast<int>(values);
}

}  // namespace

FieldHalo::FieldHalo(const HaloExchange& halo, int ncomp)
    : mHalo(halo), mNcomp(ncomp) {
  if (ncomp < 1)
    throw std::invalid_argument("FieldHalo: ncomp < 1");

  auto allocate = [this](const std::vector<HaloExchange::Op>& ops,
                         const char* label) {
    std::vector<Buffer> out;
    out.reserve(ops.size());
    for (const auto& op : ops) {
      Buffer b;
      b.n = checkedSize(op.box.numPts() * mNcomp, label);
      b.dev = Kokkos::View<Real*>("saturn::halo_buffer",
                                  static_cast<std::size_t>(b.n));
      b.host = Kokkos::create_mirror_view(b.dev);
      out.push_back(std::move(b));
    }
    return out;
  };

  mSend = allocate(mHalo.sendOps(), "FieldHalo send");
  mRecv = allocate(mHalo.recvOps(), "FieldHalo recv");
}

void FieldHalo::pack(int sendIndex, const CellField& field) {
  const HaloExchange::Op& op = mHalo.sendOps()[sendIndex];
  Buffer& buffer = mSend[sendIndex];
  const IndexBox box = op.box;
  const int nx = box.length(0);
  const int ny = box.length(1);
  const int nz = box.length(2);
  const int nc = mNcomp;
  Kokkos::View<Real*> dev = buffer.dev;
  CellField f = field;

  Kokkos::parallel_for(
      "saturn::halo_pack",
      Kokkos::MDRangePolicy<Kokkos::Rank<3>, Kokkos::IndexType<int>>(
          {0, 0, 0}, {nx, ny, nz}),
      KOKKOS_LAMBDA(int ii, int jj, int kk) {
        for (int c = 0; c < nc; ++c) {
          const std::size_t t =
              (static_cast<std::size_t>((kk * ny + jj) * nx + ii) *
               static_cast<std::size_t>(nc)) +
              static_cast<std::size_t>(c);
          dev[t] = f(box.lo[0] + ii, box.lo[1] + jj, box.lo[2] + kk, c);
        }
      });

  Kokkos::fence();
  Kokkos::deep_copy(buffer.host, buffer.dev);
}

void FieldHalo::unpack(int recvIndex, CellField& field) {
  const HaloExchange::Op& op = mHalo.recvOps()[recvIndex];
  Buffer& buffer = mRecv[recvIndex];
  Kokkos::deep_copy(buffer.dev, buffer.host);
  Kokkos::fence();

  const IndexBox box = op.box;
  const int nx = box.length(0);
  const int ny = box.length(1);
  const int nz = box.length(2);
  const int nc = mNcomp;
  Kokkos::View<Real*> dev = buffer.dev;
  CellField f = field;

  Kokkos::parallel_for(
      "saturn::halo_unpack",
      Kokkos::MDRangePolicy<Kokkos::Rank<3>, Kokkos::IndexType<int>>(
          {0, 0, 0}, {nx, ny, nz}),
      KOKKOS_LAMBDA(int ii, int jj, int kk) {
        for (int c = 0; c < nc; ++c) {
          const std::size_t t =
              (static_cast<std::size_t>((kk * ny + jj) * nx + ii) *
               static_cast<std::size_t>(nc)) +
              static_cast<std::size_t>(c);
          f(box.lo[0] + ii, box.lo[1] + jj, box.lo[2] + kk, c) = dev[t];
        }
      });
}

void FieldHalo::copyLocal(int recvIndex, CellField& field) {
  const HaloExchange::Op& op = mHalo.recvOps()[recvIndex];
  const IndexBox src =
      mHalo.partition().validSlab(field.nGrow(), -op.dx, -op.dy, -op.dz);
  const IndexBox dst = op.box;
  const int nx = dst.length(0);
  const int ny = dst.length(1);
  const int nz = dst.length(2);
  const int nc = mNcomp;
  CellField f = field;

  Kokkos::parallel_for(
      "saturn::halo_local",
      Kokkos::MDRangePolicy<Kokkos::Rank<3>, Kokkos::IndexType<int>>(
          {0, 0, 0}, {nx, ny, nz}),
      KOKKOS_LAMBDA(int ii, int jj, int kk) {
        for (int c = 0; c < nc; ++c)
          f(dst.lo[0] + ii, dst.lo[1] + jj, dst.lo[2] + kk, c) =
              f(src.lo[0] + ii, src.lo[1] + jj, src.lo[2] + kk, c);
      });
}

void FieldHalo::execute(CellField& field) {
  if (field.ncomp() != mNcomp)
    throw std::invalid_argument("FieldHalo: component count does not match");
  if (field.nGrow() != mHalo.nGrow())
    throw std::invalid_argument("FieldHalo: ghost width does not match");
  for (int d = 0; d < 3; ++d) {
    if (field.origin(d) != mHalo.partition().localOrigin(d) ||
        field.extent(d) != mHalo.partition().localExtent(d))
      throw std::invalid_argument(
          "FieldHalo: field layout does not match the partition");
  }

  // Local periodic images (also the whole periodic path on one rank).
  for (int r = 0; r < static_cast<int>(mRecv.size()); ++r)
    if (mHalo.recvOps()[r].peer == mHalo.partition().rank())
      copyLocal(r, field);

#ifdef SATURN_USE_MPI
  std::vector<MPI_Request> requests;
  requests.reserve(mSend.size() + mRecv.size());

  for (int r = 0; r < static_cast<int>(mRecv.size()); ++r) {
    const HaloExchange::Op& op = mHalo.recvOps()[r];
    if (op.peer == mHalo.partition().rank())
      continue;
    MPI_Request req = MPI_REQUEST_NULL;
    MPI_Irecv(mRecv[r].host.data(), mRecv[r].n, kMpiReal, op.peer, op.tag,
              MPI_COMM_WORLD, &req);
    requests.push_back(req);
  }

  for (int s = 0; s < static_cast<int>(mSend.size()); ++s) {
    const HaloExchange::Op& op = mHalo.sendOps()[s];
    if (op.peer == mHalo.partition().rank())
      continue;
    pack(s, field);
    MPI_Request req = MPI_REQUEST_NULL;
    MPI_Isend(mSend[s].host.data(), mSend[s].n, kMpiReal, op.peer, op.tag,
              MPI_COMM_WORLD, &req);
    requests.push_back(req);
  }

  if (!requests.empty())
    MPI_Waitall(static_cast<int>(requests.size()), requests.data(),
                MPI_STATUSES_IGNORE);

  for (int r = 0; r < static_cast<int>(mRecv.size()); ++r) {
    const HaloExchange::Op& op = mHalo.recvOps()[r];
    if (op.peer == mHalo.partition().rank())
      continue;
    unpack(r, field);
  }
#else
  (void)field;  // only local periodic copies exist without MPI
#endif

  Kokkos::fence();
}

}  // namespace saturn
