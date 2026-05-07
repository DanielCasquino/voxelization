#include <mpi.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <vector>
#include "voxelize.h"

int main(int argc, char *argv[])
{
    int rank, size, ierr;

    ierr = MPI_Init(&argc, &argv);
    if (ierr != MPI_SUCCESS)
        MPI_Abort(MPI_COMM_WORLD, ierr);

    ierr = MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    if (ierr != MPI_SUCCESS)
        MPI_Abort(MPI_COMM_WORLD, ierr);

    ierr = MPI_Comm_size(MPI_COMM_WORLD, &size);
    if (ierr != MPI_SUCCESS)
        MPI_Abort(MPI_COMM_WORLD, ierr);

    if (rank == 0)
    {
        Assimp::Importer imp;
        const aiScene *scene = imp.ReadFile(argv[1], aiProcess_Triangulate);
        const aiMesh *mesh = scene->mMeshes[0];

        Triangle *triangles = new Triangle[mesh->mNumFaces];

        for (size_t i = 0; i < mesh->mNumFaces; ++i)
        {
            for (size_t j = 0; j < 3; ++j)
            {
                const aiVector3D vertex = mesh->mVertices[mesh->mFaces[i].mIndices[j]];
                triangles[i].v[j][0] = vertex.x;
                triangles[i].v[j][1] = vertex.y;
                triangles[i].v[j][2] = vertex.z;
            }
        }

        printf("Flattened array with %d faces\n", mesh->mNumFaces);
    }

    std::vector<uint64_t> result = Voxelize();

    ierr = MPI_Finalize();
    if (ierr != MPI_SUCCESS)
        MPI_Abort(MPI_COMM_WORLD, ierr);

    return 0;
}
