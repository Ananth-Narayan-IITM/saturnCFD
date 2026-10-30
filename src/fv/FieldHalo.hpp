#pragma once

#include <vector>

#include <Kokkos_Core.hpp>

#include "comm/HaloExchange.hpp"
#include "fv/CellField.hpp"

namespace saturn {

/// Kokkos halo exchange for a CellField.
///
/// The operation list still comes from the backend-agnostic HaloExchange.
/// This adapter performs:
///
///   device pack -> host mirror -> MPI_Isend/Irecv -> device unpack
///
/// Host-staged MPI is the portable first implementation. A GPU-aware path can
/// replace the mirror without changing this interface. Buffers are allocated
/// once in the constructor, not inside a time loop. The referenced
/// HaloExchange (and its MeshPartition) must outlive this object.
class FieldHalo {
 public:
  FieldHalo(const HaloExchange& halo, int ncomp);

  FieldHalo(const FieldHalo&) = delete;
  FieldHalo& operator=(const FieldHalo&) = delete;

  /// Fill the field ghosts. Physical boundaries are not filled.
  void execute(CellField& field);

  int numSendOps() const { return static_cast<int>(mSend.size()); }
  int numRecvOps() const { return static_cast<int>(mRecv.size()); }

 private:
  struct Buffer {
    Kokkos::View<Real*> dev;
    Kokkos::View<Real*, Kokkos::HostSpace> host;
    int n = 0;
  };

  const HaloExchange& mHalo;
  int mNcomp = 1;
  std::vector<Buffer> mSend;
  std::vector<Buffer> mRecv;

  void pack(int sendIndex, const CellField& field);
  void unpack(int recvIndex, CellField& field);
  void copyLocal(int recvIndex, CellField& field);
};

}  // namespace saturn
