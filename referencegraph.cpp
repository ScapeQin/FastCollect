/*
 * Copyright (c) 1997, 2013, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 *
 */

#include "precompiled.hpp"
#include "compiler/compileBroker.hpp"
#include "gc_implementation/shared/gcTimer.hpp"
#include "gc_implementation/shared/gcTrace.hpp"
#include "gc_implementation/shared/markSweep.inline.hpp"
#include "gc_interface/collectedHeap.inline.hpp"
#include "oops/methodData.hpp"
#include "oops/objArrayKlass.inline.hpp"
#include "oops/oop.inline.hpp"
#include "oops/klass.hpp"
#include <stack>

map<oopDesc*, ReferenceGraphNode*> ReferenceGraphNode::mapOOPDescGraph = map<oopDesc*, ReferenceGraphNode*>();
ReferenceGraphNode* ReferenceGraphNode::root = create (NULL );
stack<ReferenceGraphNode*> ReferenceGraphNode::graph_create_stack = stack<ReferenceGraphNode*>();
GPUReferenceGraphNode* GPUReferenceGraph::array_gpu_graph_nodes = NULL;
vector<int> GPUReferenceGraph::adjacent_vertices = vector<int> ();
    
int ReferenceGraphNode::addField (oopDesc* obj, oopDesc* prev, oopDesc* field)
{
    ReferenceGraphNode* objNode = ReferenceGraphNode::create(obj);
    ReferenceGraphNode* fieldNode = ReferenceGraphNode::create(field);
    
    //objNode->_children.remove (mapOOPDescGraph[prev]);
    objNode->_addChild (fieldNode);
    return 1;
}

ReferenceGraphNode* ReferenceGraphNode::create (oopDesc* _oop)
{
  if (_oop)
  {
    ReferenceGraphNode* p = mapOOPDescGraph[_oop];
    if (p)
      return p;
    else
      return new ReferenceGraphNode (_oop, NULL);  
  }
  else
    return new ReferenceGraphNode (_oop, NULL);
}
int ReferenceGraphNode::n_young_objects = 0;

static void print_indent (int indent)
{
  int i;
  
  for (i = 0; i < indent; i++)
    putchar (' ');
    
}
void GPUReferenceGraphNode::print (int indent)
{
  print_indent (indent);
  printf ("%d\n", index);
  
  int i;
  
  for (i = first_index; i < last_index; i++)
  {
  //printf ("%d %d\n", i, GPUReferenceGraph::adjacent_vertices [i]);
    if (GPUReferenceGraph::adjacent_vertices [i] == -1 || GPUReferenceGraph::adjacent_vertices [i] == index)
      continue;
    
    GPUReferenceGraph::array_gpu_graph_nodes[GPUReferenceGraph::adjacent_vertices [i]].print (indent + 2);
  }
}

void ReferenceGraphNode::toGPUReferenceGraph ()
{
  int i = 0;
  map<oopDesc*, ReferenceGraphNode*>::iterator it;
  //std::cout<<"new " << std::endl;
  GPUReferenceGraph::array_gpu_graph_nodes = new GPUReferenceGraphNode [mapOOPDescGraph.size ()];
 //std::cout<<"new done " << std::endl;
  for (it = mapOOPDescGraph.begin (); it != mapOOPDescGraph.end(); it++)
  {
    if(it->second)
    {
      it->second->index = i;
      GPUReferenceGraph::array_gpu_graph_nodes [i].index = i;
      GPUReferenceGraph::array_gpu_graph_nodes [i].marked = 0;
      //GPUReferenceGraph::array_gpu_graph_nodes [i].address = (uint64_t)(HeapWord*)it->second->_oop;
      i++;
    }
  }
  //std::cout<<"create set " << std::endl;
  for (it = mapOOPDescGraph.begin (); it != mapOOPDescGraph.end(); it++)
  {
    if (it->second)
    {
      i = it->second->index;
      GPUReferenceGraph::array_gpu_graph_nodes [i].first_index = GPUReferenceGraph::adjacent_vertices.size ();
      
      for (set<ReferenceGraphNode*>::iterator k = it->second->_children.begin (); k != it->second->_children.end ();
            ++k)
      {
        if ((*k)->index != -1)
          GPUReferenceGraph::adjacent_vertices.push_back ((*k)->index);
      }
    
      GPUReferenceGraph::array_gpu_graph_nodes [i].last_index = GPUReferenceGraph::adjacent_vertices.size ();
    }
  }
  
  //std::cout<< "CHECKING "<< std::endl;
  
  /*for (it = mapOOPDescGraph.begin (); it != mapOOPDescGraph.end(); it++)
  {
    i = it->second->index;
   
    for (size_t k = 0; k < it->second->_children.size (); k++)
    {
      int l;
      
      l = GPUReferenceGraph::array_gpu_graph_nodes [i].first_index;
      
      if (GPUReferenceGraph::adjacent_vertices[l] != it->second->_children[k]->index)
      {
        std::cout<< "PROBLEM "<< std::endl;  
      }
      
      l++;
    }
  }*/
  
}

ReferenceGraphNode* ReferenceGraphNode::isChild (oopDesc* p)
{    
  for (set<ReferenceGraphNode*>::iterator k = _children.begin (); k != _children.end ();
          ++k)
    {
      if ((*k)->_oop == p)
        return *k;
    }
  
  return NULL;
}

ReferenceGraphNode::ReferenceGraphNode (oopDesc* _oop, ReferenceGraphNode* parent)
    {
      this->_oop = _oop;
      index = -1;
      if (_oop != NULL)
        mapOOPDescGraph [_oop] = this;
      this->parent = parent;
      traversed = false;
    }
    
    ReferenceGraphNode* ReferenceGraphNode::_addChild (ReferenceGraphNode* child)
    {
       _children.insert (child);
      return child;
    }
    
    ReferenceGraphNode* ReferenceGraphNode::addChild (oop* p) 
    {
      oop heap_oop = oopDesc::load_heap_oop(p);
      if (!oopDesc::is_null(heap_oop)) {
        oop obj = oopDesc::decode_heap_oop_not_null(heap_oop);
        return _addChild (obj);
      }
      
      return NULL;
    }
    
    ReferenceGraphNode* ReferenceGraphNode::addChild (narrowOop* p) 
    {
      narrowOop heap_oop = oopDesc::load_heap_oop(p);
      if (!oopDesc::is_null(heap_oop)) {
        oop obj = oopDesc::decode_heap_oop_not_null(heap_oop);
        return _addChild (obj);
      }
      
      return NULL;
    }
    
    ReferenceGraphNode* ReferenceGraphNode::_addChild (oopDesc* _oop)
    {
      ReferenceGraphNode* child;
      
      child = mapOOPDescGraph[_oop];
      if (this == root)
      {
        //std::cout<< "root child "<< child << " " << child->_children.size() << std::endl;
      }
      
      if (child == NULL)
      {
        child = new ReferenceGraphNode (_oop, this);
        mapOOPDescGraph[_oop] = child;
      }
      child->parent = this;
      return _addChild (child);
    }
    
    ReferenceGraphNode* ReferenceGraphNode::getNodeForOOP (oopDesc* _oopDesc)
    {
      return mapOOPDescGraph[_oopDesc];
    }
    
    void ReferenceGraphNode::clearMapOOPDescGraph ()
    {
      map<oopDesc*, ReferenceGraphNode*>::iterator it;
      
      if (mapOOPDescGraph.size () >= 1)
      {
        for (it = mapOOPDescGraph.begin (); it != mapOOPDescGraph.end(); it++)
        {
          delete it->second;
        }

        mapOOPDescGraph.clear ();
        
        GPUReferenceGraph::adjacent_vertices.clear ();
        delete GPUReferenceGraph::array_gpu_graph_nodes;
      }
      
      root->_children.clear ();
    }
#endif
