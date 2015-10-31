/*
 *  get_transitive_closure.h
 *
 */

// This file was modified from get_transitive_closure.hpp in Boost.
// The original copyright info is listed below

// Copyright (C) 2001 Vladimir Prus <ghost@cs.msu.su>
// Copyright (C) 2001 Jeremy Siek <jsiek@cs.indiana.edu>
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GRAPH_TRANSITIVE_CLOSURE_HPP
#define BOOST_GRAPH_TRANSITIVE_CLOSURE_HPP


#include <vector>
#include <algorithm> // for std::min and std::max
#include <functional>
#include <boost/config.hpp>
#include <boost/bind.hpp>
#include <boost/graph/vector_as_graph.hpp>
#include <boost/graph/strong_components.hpp>
#include <boost/graph/topological_sort.hpp>
#include <boost/graph/graph_concepts.hpp>
#include <boost/graph/named_function_params.hpp>

using namespace boost;

typedef uint16_t v_id_size_type;

inline void
union_successor_sets(const std::vector < v_id_size_type > &s1,
                     const std::vector < v_id_size_type > &s2,
                     std::vector < v_id_size_type > &s3) {
  BOOST_USING_STD_MIN();
  for (std::size_t k = 0; k < s1.size(); ++k)
    s3[k] = min BOOST_PREVENT_MACRO_SUBSTITUTION(s1[k], s2[k]);
}


namespace detail {

template < typename TheContainer, typename ST = std::size_t,
         typename VT = typename TheContainer::value_type >
struct subscript_t:public std::unary_function < ST, VT > {
  typedef VT& result_type;

  subscript_t(TheContainer & c):container(&c) {
  }
  VT & operator() (const ST & i) const {
    return (*container)[i];
  }
protected:
  TheContainer * container;
};
template < typename TheContainer >
subscript_t < TheContainer > subscript(TheContainer & c) {
  return subscript_t < TheContainer > (c);
}

} // namespace detail

template < typename Graph,
         typename GraphTC,
         typename G_to_TC_VertexMap,
         typename VertexIndexMap >
void get_transitive_closure(const Graph & g, GraphTC & tc,
                            G_to_TC_VertexMap g_to_tc_map,
                            VertexIndexMap index_map) {
  if (num_vertices(g) == 0)
    return;
  typedef typename graph_traits < Graph >::vertex_descriptor vertex;
  typedef typename graph_traits < Graph >::edge_descriptor edge;
  typedef typename graph_traits < Graph >::vertex_iterator vertex_iterator;
  //typedef typename property_traits < VertexIndexMap >::value_type size_type;
  typedef typename graph_traits < Graph >::adjacency_iterator adjacency_iterator;
  function_requires < VertexListGraphConcept < Graph > >();
  function_requires < AdjacencyGraphConcept < Graph > >();
  function_requires < VertexMutableGraphConcept < GraphTC > >();
  function_requires < EdgeMutableGraphConcept < GraphTC > >();
  function_requires < ReadablePropertyMapConcept < VertexIndexMap, vertex > >();
  typedef v_id_size_type cg_vertex;
  std::vector < cg_vertex > component_number_vec(num_vertices(g));
  iterator_property_map < cg_vertex *, VertexIndexMap, cg_vertex, cg_vertex& >
  component_number(&component_number_vec[0], index_map);
  //int num_scc = strong_components(g, component_number, vertex_index_map(index_map));
  size_t cn = 0;
  vertex_iterator cu, cu_end;
  for (boost::tie(cu, cu_end) = vertices(g); cu != cu_end; ++cu) {
    component_number[*cu] = cn++;
    //fprintf(stderr, "%d\n", component_number[*cu]);
  }
  std::vector < std::vector < vertex > >components;
  build_component_lists(g, num_vertices(g), component_number, components);
  typedef std::vector<std::vector<cg_vertex> > CG_t;
  CG_t CG(num_vertices(g));
  for (cg_vertex s = 0; s < components.size(); ++s) {
    std::vector < cg_vertex > adj;
    vertex u = components[s][0];
    adjacency_iterator v, v_end;
    for (boost::tie(v, v_end) = adjacent_vertices(u, g); v != v_end; ++v) {
      cg_vertex t = component_number[*v];
      if (s != t)           // Avoid loops in the condensation graph
        adj.push_back(t);
    }
    std::sort(adj.begin(), adj.end());
    typename std::vector<cg_vertex>::iterator di =
      std::unique(adj.begin(), adj.end());
    if (di != adj.end())
      adj.erase(di, adj.end());
    CG[s] = adj;
  }
  std::vector<cg_vertex> topo_order;
  std::vector<cg_vertex> topo_number(num_vertices(CG));
  topological_sort(CG, std::back_inserter(topo_order),
                   vertex_index_map(identity_property_map()));
  std::reverse(topo_order.begin(), topo_order.end());
  v_id_size_type n = 0;
  for (typename std::vector<cg_vertex>::iterator iter = topo_order.begin();
       iter != topo_order.end(); ++iter)
    topo_number[*iter] = n++;
  for (size_t i = 0; i < num_vertices(CG); ++i)
    std::sort(CG[i].begin(), CG[i].end(),
              boost::bind(std::less<cg_vertex>(),
                          boost::bind(::detail::subscript(topo_number), _1),
                          boost::bind(::detail::subscript(topo_number), _2)));
  std::vector<std::vector<cg_vertex> > chains;
  {
    std::vector<cg_vertex> in_a_chain(num_vertices(CG));
    for (typename std::vector<cg_vertex>::iterator i = topo_order.begin();
         i != topo_order.end(); ++i) {
      cg_vertex v = *i;
      if (!in_a_chain[v]) {
        chains.resize(chains.size() + 1);
        std::vector<cg_vertex>& chain = chains.back();
        for (;;) {
          chain.push_back(v);
          in_a_chain[v] = true;
          typename graph_traits<CG_t>::adjacency_iterator adj_first, adj_last;
          boost::tie(adj_first, adj_last) = adjacent_vertices(v, CG);
          typename graph_traits<CG_t>::adjacency_iterator next
            = std::find_if(adj_first, adj_last,
                           std::not1(::detail::subscript(in_a_chain)));
          if (next != adj_last)
            v = *next;
          else
            break;            // end of chain, dead-end
        }
      }
    }
  }
  std::vector<v_id_size_type> chain_number(num_vertices(CG));
  std::vector<v_id_size_type> pos_in_chain(num_vertices(CG));
  for (size_t i = 0; i < chains.size(); ++i)
    for (size_t j = 0; j < chains[i].size(); ++j) {
      cg_vertex v = chains[i][j];
      chain_number[v] = i;
      pos_in_chain[v] = j;
    }
  cg_vertex inf = (std::numeric_limits< cg_vertex >::max)();
  std::vector<std::vector<cg_vertex> > successors(num_vertices(CG),
      std::vector<cg_vertex>
      (chains.size(), inf));
  for (typename std::vector<cg_vertex>::reverse_iterator
       i = topo_order.rbegin(); i != topo_order.rend(); ++i) {
    cg_vertex u = *i;
    typename graph_traits<CG_t>::adjacency_iterator adj, adj_last;
    for (boost::tie(adj, adj_last) = adjacent_vertices(u, CG);
         adj != adj_last; ++adj) {
      cg_vertex v = *adj;
      if (topo_number[v] < successors[u][chain_number[v]]) {
        // Succ(u) = Succ(u) U Succ(v)
        union_successor_sets(successors[u], successors[v], successors[u]);
        // Succ(u) = Succ(u) U {v}
        successors[u][chain_number[v]] = topo_number[v];
      }
    }
  }
  for (size_t i = 0; i < CG.size(); ++i)
    CG[i].clear();
  for (size_t i = 0; i < CG.size(); ++i)
    for (size_t j = 0; j < chains.size(); ++j) {
      size_t topo_num = successors[i][j];
      if (topo_num < inf) {
        cg_vertex v = topo_order[topo_num];
        for (size_t k = pos_in_chain[v]; k < chains[j].size(); ++k)
          CG[i].push_back(chains[j][k]);
      }
    }
  // Add vertices to the transitive closure graph
  typedef typename graph_traits < GraphTC >::vertex_descriptor tc_vertex;
  {
    vertex_iterator i, i_end;
    for (boost::tie(i, i_end) = boost::vertices(g); i != i_end; ++i)
      g_to_tc_map[*i] = add_vertex(tc);
  }
  // Add edges between all the vertices in two adjacent SCCs
  typename graph_traits<CG_t>::vertex_iterator si, si_end;
  for (boost::tie(si, si_end) = boost::vertices(CG); si != si_end; ++si) {
    cg_vertex s = *si;
    typename graph_traits<CG_t>::adjacency_iterator i, i_end;
    for (boost::tie(i, i_end) = adjacent_vertices(s, CG); i != i_end; ++i) {
      cg_vertex t = *i;
      for (size_t k = 0; k < components[s].size(); ++k)
        for (size_t l = 0; l < components[t].size(); ++l)
          add_edge(g_to_tc_map[components[s][k]],
                   g_to_tc_map[components[t][l]], tc);
    }
  }
  // Add edges connecting all vertices in a SCC
  for (size_t i = 0; i < components.size(); ++i)
    if (components[i].size() > 1)
      for (size_t k = 0; k < components[i].size(); ++k)
        for (size_t l = 0; l < components[i].size(); ++l) {
          vertex u = components[i][k], v = components[i][l];
          add_edge(g_to_tc_map[u], g_to_tc_map[v], tc);
        }
  // Find loopbacks in the original graph.
  // Need to add it to transitive closure.
  {
    vertex_iterator i, i_end;
    for (boost::tie(i, i_end) = vertices(g); i != i_end; ++i) {
      adjacency_iterator ab, ae;
      for (boost::tie(ab, ae) = adjacent_vertices(*i, g); ab != ae; ++ab) {
        if (*ab == *i)
          if (components[component_number[*i]].size() == 1)
            add_edge(g_to_tc_map[*i], g_to_tc_map[*i], tc);
      }
    }
  }
}

template <typename Graph, typename GraphTC>
void get_transitive_closure(const Graph & g, GraphTC & tc) {
  if (num_vertices(g) == 0)
    return;
  typedef typename property_map<Graph, vertex_index_t>::const_type
  VertexIndexMap;
  VertexIndexMap index_map = get(vertex_index, g);
  typedef typename graph_traits<GraphTC>::vertex_descriptor tc_vertex;
  std::vector<tc_vertex> to_tc_vec(num_vertices(g));
  iterator_property_map < tc_vertex *, VertexIndexMap, tc_vertex, tc_vertex&>
  g_to_tc_map(&to_tc_vec[0], index_map);
  get_transitive_closure(g, tc, g_to_tc_map, index_map);
}


template < typename Graph, typename GraphTC, typename G_to_TC_VertexMap,
         typename VertexIndexMap>
void transitive_closure_dispatch
(const Graph & g, GraphTC & tc,
 G_to_TC_VertexMap g_to_tc_map, VertexIndexMap index_map) {
  typedef typename graph_traits < GraphTC >::vertex_descriptor tc_vertex;
  typename std::vector < tc_vertex >::size_type
  n = is_default_param(g_to_tc_map) ? num_vertices(g) : 1;
  std::vector < tc_vertex > to_tc_vec(n);
  get_transitive_closure
  (g, tc,
   choose_param(g_to_tc_map, make_iterator_property_map
                (to_tc_vec.begin(), index_map, to_tc_vec[0])),
   index_map);
}


template < typename Graph, typename GraphTC,
         typename P, typename T, typename R >
void get_transitive_closure(const Graph & g, GraphTC & tc,
                            const bgl_named_params < P, T, R > &params) {
  if (num_vertices(g) == 0)
    return;
  transitive_closure_dispatch
  (g, tc, get_param(params, orig_to_copy_t()),
   choose_const_pmap(get_param(params, vertex_index), g, vertex_index) );
}


template < typename G > void warshall_transitive_closure(G & g) {
  typedef typename graph_traits < G >::vertex_descriptor vertex;
  typedef typename graph_traits < G >::vertex_iterator vertex_iterator;
  function_requires < AdjacencyMatrixConcept < G > >();
  function_requires < EdgeMutableGraphConcept < G > >();
  // Matrix form:
  // for k
  //  for i
  //    if A[i,k]
  //      for j
  //        A[i,j] = A[i,j] | A[k,j]
  vertex_iterator ki, ke, ii, ie, ji, je;
  for (tie(ki, ke) = vertices(g); ki != ke; ++ki)
    for (tie(ii, ie) = vertices(g); ii != ie; ++ii)
      if (edge(*ii, *ki, g).second)
        for (tie(ji, je) = vertices(g); ji != je; ++ji)
          if (!edge(*ii, *ji, g).second && edge(*ki, *ji, g).second) {
            add_edge(*ii, *ji, g);
          }
}


template < typename G > void warren_transitive_closure(G & g) {
  using namespace boost;
  typedef typename graph_traits < G >::vertex_descriptor vertex;
  typedef typename graph_traits < G >::vertex_iterator vertex_iterator;
  function_requires < AdjacencyMatrixConcept < G > >();
  function_requires < EdgeMutableGraphConcept < G > >();
  // Make sure second loop will work
  if (num_vertices(g) == 0)
    return;
  // for i = 2 to n
  //    for k = 1 to i - 1
  //      if A[i,k]
  //        for j = 1 to n
  //          A[i,j] = A[i,j] | A[k,j]
  vertex_iterator ic, ie, jc, je, kc, ke;
  for (tie(ic, ie) = vertices(g), ++ic; ic != ie; ++ic)
    for (tie(kc, ke) = vertices(g); *kc != *ic; ++kc)
      if (edge(*ic, *kc, g).second)
        for (tie(jc, je) = vertices(g); jc != je; ++jc)
          if (!edge(*ic, *jc, g).second && edge(*kc, *jc, g).second) {
            add_edge(*ic, *jc, g);
          }
  //  for i = 1 to n - 1
  //    for k = i + 1 to n
  //      if A[i,k]
  //        for j = 1 to n
  //          A[i,j] = A[i,j] | A[k,j]
  for (tie(ic, ie) = vertices(g), --ie; ic != ie; ++ic)
    for (kc = ic, ke = ie, ++kc; kc != ke; ++kc)
      if (edge(*ic, *kc, g).second)
        for (tie(jc, je) = vertices(g); jc != je; ++jc)
          if (!edge(*ic, *jc, g).second && edge(*kc, *jc, g).second) {
            add_edge(*ic, *jc, g);
          }
}


#endif


