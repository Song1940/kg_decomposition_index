# kg_decomposition_index

# kgcore_index
This is the implementation of kg-core index, which is described in the following papaer submitted EDBT 2026:
- Efficient Locality-based Indexing for Cohesive Subgraphs Discovery in Hypergraphs

## This repository contains the index construction algorithms and query processing algorithms for each index.

### Index construction
- There are four types of indexing tree
  - Naive(naive)
  - Horizontal(hori)
  - Vertical(vert)
  - Diagonal(diag)


### Query processing
- With the indexing tree you just builts, you can query to achieve (k,g)-core of the hypergraph
- Input parameters
  - Index type
  - k
  - g
- This will return
  - the size of core(number of nodes in the core you queried)
  - every each node


