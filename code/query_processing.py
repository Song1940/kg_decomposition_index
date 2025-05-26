import networkx as nx
import time
import os
import argparse
import copy
from types import MappingProxyType
import pickle
# from pympler import asizeof
import operator

class TreeNode:
    def __init__(self, name):
        self.aux ={}
        self.name = name
        self.children = []
        self.next = None
        self.value = set()
        self.jump = None

def load_hypergraph(file_path):
    hypergraph = nx.Graph()  # Create an empty hypergraph
    E = list()

    with open(file_path, 'r') as file:
        for line_number, line in enumerate(file, start=1):
            # Use set to ignore duplicate values in each line and strip whitespace from node names
            nodes = {node.strip() for node in line.strip().split(' ')}
            nodes = {int(x) for x in  nodes}
            hyperedge = set(nodes)  # Use frozenset to represent the hyperedge
            E.append(hyperedge)
            for node in nodes:
                if node not in hypergraph.nodes():
                    hypergraph.add_node(node, hyperedges=list())  # Add a node for each node
                hypergraph.nodes[node]['hyperedges'].append(hyperedge)  # Add the hyperedge to the node's hyperedge set

    return hypergraph, E

def querying_for_naive_index(tree, k,g):
    max_g = len(tree.children)
    if g > max_g:
        return None
    max_k = len(tree.children[g - 1].children)
    if k > max_k:
        return None
    else:
        core = tree.children[g-1].children[k-1].value

    return core

def querying_for_one_level(tree, k,g):
    max_g = len(tree.children)
    if g > max_g:
        return None
    max_k = len(tree.children[g - 1].children)
    if k > max_k:
        return None
    else:
        core = set()
        header = tree.children[g-1].children[k-1]
        while header:
            core = core.union(header.value)
            header = header.next

    return core


def querying_for_two_level(tree, k,g):
    max_g = len(tree.children)
    if g > max_g:
        return None
    max_k = len(tree.children[g - 1].children)
    if k > max_k:
        return None
    else:

        starters = []
        header = tree.children[g-1].children[k-1]

        while header:
            starters.append(header)
            header = header.jump

        core = set()
        for s in starters:
            while s:
                core = core.union(s.value)
                s = s.next
    return core


def querying_for_diagonal(tree, k, g):
    max_g = len(tree.children)
    if g > max_g:
        return None
    max_k = len(tree.children[g - 1].children)
    if k > max_k:
        return None
    else:
        starters = []
        header = tree.children[g - 1].children[k - 1]

        while header:
            starters.append(header)
            header = header.jump

        core = set()
        for s in range(len(starters)):
            head = starters[s]
            core.update(head.value)
            if s != 0:
                for i in range(1, s + 1):
                    # aux[i]가 존재할 경우에만 집합에 추가
                    aux_value = head.aux.get(i)
                    if aux_value:
                        core.update(aux_value)
            head = head.next
            cnt = 1
            while head:
                core.update(head.value)
                for i in range(1, cnt + 1):
                    # aux[i]가 존재할 경우에만 집합에 추가
                    aux_value = head.aux.get(i)
                    if aux_value:
                        core.update(aux_value)
                head = head.next
                cnt += 1

    return core

def neighbour_count_map(hypergraph, v,g):
    neighbor_counts = {}
    for hyperedge in hypergraph.nodes[v]['hyperedges']:
        # Increment the count for each neighbor in the hyperedge
        for neighbor in hyperedge:
            if neighbor != v:
                neighbor_counts[neighbor] = neighbor_counts.get(neighbor, 0) + 1

    filtered_neighbors = {neighbor: count for neighbor, count in neighbor_counts.items() if count >= g}


    return filtered_neighbors

def kg_core(hypergraph,k,g):
    cnt = 0
    changed = True
    H = set(hypergraph.nodes)
    while changed:
        changed = False
        nodes = H.copy()
        for v in nodes:
            cnt += 1
            map = neighbour_count_map(hypergraph,v,g)
            map =  {neighbor: count for neighbor, count in map.items() if neighbor in nodes}
            if len(map) < k:
                changed = True
                H -= {v}
  
    return H
