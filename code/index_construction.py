
import networkx as nx
import time
import os
import argparse
import copy
from types import MappingProxyType
import pickle
from pympler import asizeof
import operator
import sys

sys.setrecursionlimit(10000)

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
            nodes = {node.strip() for node in line.strip().split(',')}
            nodes = {int(x) for x in  nodes}
            hyperedge = set(nodes)  # Use frozenset to represent the hyperedge
            E.append(hyperedge)
            for node in nodes:
                if node not in hypergraph.nodes():
                    hypergraph.add_node(node, hyperedges=list())  # Add a node for each node
                hypergraph.nodes[node]['hyperedges'].append(hyperedge)  # Add the hyperedge to the node's hyperedge set

    return hypergraph, E

"""hypergraph에 속한 각각 노드에 대해 이웃 노드들과, 이웃 노드들과의 co-occurence를 반환하는 함수"""
def neighbour_count_map(hypergraph, v,g):
    neighbor_counts = {}
    for hyperedge in hypergraph.nodes[v]['hyperedges']:
        # Increment the count for each neighbor in the hyperedge
        for neighbor in hyperedge:
            if neighbor != v:
                neighbor_counts[neighbor] = neighbor_counts.get(neighbor, 0) + 1

    filtered_neighbors = {neighbor: count for neighbor, count in neighbor_counts.items() if count >= g}


    return filtered_neighbors


def get_neighbour(hypergraph, v):
    neighbor_set= set()
    for hyperedge in hypergraph.nodes[v]['hyperedges']:
        # Increment the count for each neighbor in the hyperedge
        for neighbor in hyperedge:
            if neighbor != v:
                neighbor_set.add(neighbor)

    return neighbor_set



def get_induced_subhypergraph(hypergraph, node_set):
    subhypergraph = nx.Graph()
    for node in node_set:
        if node in hypergraph.nodes:
            subhypergraph.add_node(node, hyperedges=[])
            for hyperedge in hypergraph.nodes[node]['hyperedges']:
                p = node_set & set(hyperedge)
                if len(p) >= 2:
                    subhypergraph.add_edges_from([(u, v) for u in p for v in p if u != v])
                    subhypergraph.nodes[node]['hyperedges'].append(p)
    return subhypergraph

""" (k,g)-core peeling algorithm"""
def find_kg_core(hypergraph,k,g):
    changed = True
    H = set(hypergraph.nodes)
    while changed:
        changed = False
        nodes = H.copy()
        for v in nodes:
            map = neighbour_count_map(hypergraph,v,g)
            map =  {neighbor: count for neighbor, count in map.items() if neighbor in nodes}
            if len(map) < k:
                changed = True
                H -= {v}
    return H



def kg_core_peeling(hypergraph,k,g):
    changed = True
    H = set(hypergraph.nodes())
    T = set()
    while changed:
        changed = False
        if T == set():
            nodes = H.copy()
        else:
            nodes = H.intersection(T)
        for v in nodes:
            T = T - {v}
            map = neighbour_count_map(hypergraph,v,g)
            map =  {neighbor: count for neighbor, count in map.items() if neighbor in nodes}
            if len(map) < k:
                changed = True
                H -= {v}
                T.union(get_neighbour(hypergraph,v))
    return H



def enumerate_kg_core_fixing_g(hypergraph, g):
    H = set(hypergraph.nodes)
    S = []
    T = set()
    for k in range(1,len(hypergraph.nodes)):
        if len(H) <= k:
            break
        while True:
            if len(H) <= k:
                break
            changed = False
            nodes = H.copy()
            if T == set():
                for v in nodes:
                    T = T - {v}
                    map = neighbour_count_map(hypergraph,v,g)
                    map = {neighbor: count for neighbor, count in map.items() if neighbor in nodes}
                    if len(map) < k:
                        H -= {v}
                        changed = True
                        T.union(get_neighbour(hypergraph,v))
            else:
                for v in nodes.intersection(T):
                    T = T - {v}
                    map = neighbour_count_map(hypergraph,v,g)
                    map = {neighbor: count for neighbor, count in map.items() if neighbor in nodes}
                    if len(map) < k:
                        H -= {v}
                        changed = True
                        T.union(get_neighbour(hypergraph,v))
            if not changed:
                S.append(H.copy())
                T = set()
                break

    return S




def naive_index_construction(hypergraph,E):
    T = TreeNode("root")
    for g in range(1,len(E)):
        S = enumerate_kg_core_fixing_g(hypergraph,g)
        if len(S) == 0:
            break
        T.children.append(TreeNode(g))
        for s in range(len(S)):
            T.children[g-1].children.append(TreeNode((s+1,g)))
            T.children[g - 1].children[s].value = S[s]

    return T




def enumerate_1_g(hypergraph, g):
    H = set(hypergraph.nodes)
    S = []
    T = set()
    temp = set()
    for k in range(1,len(hypergraph.nodes)):
        if len(H) <= k:
            break
        while True:
            if len(H) <= k:
                break
            changed = False
            nodes = H.copy()
            if T != set():
                nodes = nodes.intersection(T)
            for v in nodes:
                T = T - {v}
                map = neighbour_count_map(hypergraph,v,g)
                map = {neighbor: count for neighbor, count in map.items() if neighbor in nodes}
                if len(map) < k:
                    H -= {v}
                    changed = True
            if not changed:
                if len(temp) != 0:
                    S.append(temp.difference(H))
                    T = set()
                temp = H.copy()
                break
    if len(temp) != 0:
        S.append(temp)


    return S



def one_level_compression(hypergraph, E):
    T = TreeNode("root")
    for g in range(1, len(E)):
        S = enumerate_1_g(hypergraph, g)
        if len(S) == 0:
            break
        T.children.append(TreeNode(g))
        prev = TreeNode(None)
        for s in range(len(S)):
            T.children[g - 1].children.append(TreeNode((s + 1, g)))
            T.children[g - 1].children[s].value = S[s]
            u = T.children[g-1].children[s]
            if prev.name != None:
                prev.next = u
            prev = u

    return T

def jump_compression(hypergraph,E):
    h_time_start = time.time()
    T_1 = one_level_compression(hypergraph,E)
    h_time_end = time.time()
    T_2 = T_1
    max_g = len(T_2.children)
    for g in range(max_g-1):
        max_k = len(T_2.children[g+1].children)
        for k in range(max_k):
            T_2.children[g].children[k].jump = T_2.children[g+1].children[k]
            T_2.children[g].children[k].value = T_2.children[g].children[k].value.difference(T_2.children[g].children[k].jump.value)

    h_time = h_time_end - h_time_start
    return T_2, h_time

def diagonal_compression(hypergraph,E):
    v_time_start = time.time()
    T,h_time = jump_compression(hypergraph,E)
    v_time_end = time.time()
    for g in range(len(T.children)):
        head = T.children[g].children[0] 
        if head.next == None:
            break
        for k in range(len(T.children[g].children)-1):
            if head.next and head.jump : 
                diag = head.jump
                head = head.next
                intersect = head.value.intersection(diag.value) 

                if not diag.next:
                    head.jump = TreeNode("aux") 
                    diag.next = head.jump

                diag.next.aux[1] = intersect
                for i in diag.aux.keys():
                    try :
                        diag.next.aux[i+1] = diag.aux[i].intersection(head.aux[i])
                        head.aux[i] = head.aux[i].difference(diag.next.aux[i + 1])
                    except KeyError:
                        continue

                head.value = head.value.difference(intersect)
                if head.next and head.next.aux:
                    head.value = head.value.difference(head.next.aux[1])

            else: 
                while head.next:
                    for i in head.next.aux.keys():
                        if i ==1:
                            head.value = head.value.difference(head.next.aux[i])
                        else:
                            head.aux[i-1] = head.aux[i-1].difference(head.next.aux[i])
                    head = head.next

    v_time = v_time_end - v_time_start
   

    return T, h_time, v_time

def count_total_nodes(tree, type):
    if type == "naive":
        total = 0
        for i in range(len(tree.children)):
            for j in range(len(tree.children[i].children)):
                total += len(tree.children[i].children[j].value)
        return total

    total = 0
    if type == 'diag':
        for i in range(len(tree.children)):
            head = tree.children[i].children[0]
            while head:
                for j in head.aux.keys():
                    total += len(head.aux[j])
                total += len(head.value)
                head = head.next
        return total
    else:
        for i in range(len(tree.children)):
            head = tree.children[i].children[0]
            while head:
                total += len(head.value)
                head = head.next
        return total

def count_each_nodes(tree, type):
    count_map =dict()
    if type == 'diag':
        for i in range(len(tree.children)):
            head = tree.children[i].children[0]
            while head:
                for j in head.aux.keys():
                    for k in head.aux[j]:
                        try:
                            count_map[k] += 1
                        except KeyError:
                            count_map[k] = 1

                for j in head.value:
                    try:
                        count_map[j] += 1
                    except KeyError:
                        count_map[j] = 1
                head = head.next

        count_map = dict(sorted(count_map.items(), key=operator.itemgetter(1), reverse=True))
        return count_map

    else:
        for i in range(len(tree.children)):
            for j in range(len(tree.children[i].children)):
                for k in tree.children[i].children[j].value:
                    try:
                        count_map[k] += 1
                    except KeyError:
                        count_map[k] = 1

        count_map = dict(sorted(count_map.items(), key=operator.itemgetter(1), reverse=True))
        return count_map

def count_empty_leaf(tree):
    map = {}
    for i in range(len(tree.children)):
        count = 0
        head = tree.children[i].children[0]
        while head:
            if (len(head.value) + len(head.aux)) == 0:
                count += 1
            head = head.next

        map[i+1]  = count
    return map
