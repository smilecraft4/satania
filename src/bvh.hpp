#pragma once

#include "mesh.hpp"
#include "timer.hpp"
#include "aabb.hpp"

#include <glm/glm.hpp>
#include <vector>
#include <stack>
#include <algorithm>

#define SATANIA_BVH_CLUSTER_COLORED 0
#define SATANIA_BVH_TIMER 0

class BVH
{
public:

	enum Axis
	{
		AXIS_X,
		AXIS_Y,
		AXIS_Z,
		AXIS_MAX
	};

	struct Triangle
	{
		Vertex              vertices[3];
	};

	struct Node
	{
		AABB                aabb;
		int                 node_right;
		int                 node_left;
		int                 leaf;
		int                 leaf_elem;
	};

	std::vector<Triangle>   m_triangles;
	std::vector<Node>       m_nodes;

	BVH(const Mesh& mesh, int leaf_max_size = 4, int depth_max_size = 512) : m_leaf_max_size{ leaf_max_size }, m_depth_max_size{ depth_max_size }
	{
		m_indices = {};
		m_midpoints = {};

		m_triangles.resize(mesh.elements.size() / 3);
		for (size_t i = 0; i < m_triangles.size(); i++)
		{
			m_triangles[i].vertices[0] = mesh.vertices[mesh.elements[i * 3 + 0]];
			m_triangles[i].vertices[1] = mesh.vertices[mesh.elements[i * 3 + 1]];
			m_triangles[i].vertices[2] = mesh.vertices[mesh.elements[i * 3 + 2]];
		}

		m_nodes.reserve(m_triangles.size());
		m_indices.push(0);
		m_root_node = buildNode(0, m_triangles.size(), AXIS_X);
	}

private:

	size_t buildNode(int first, int last, Axis axis)
	{
		// 1. Create a new node to the m_node vector
		// 2. Get the new node index and store it to the top of a stack
		// 3. Calculate the AABB of the node triangle range and store it to this node AABB
		// 4. If (the number of triangles is lower than the triangle size or the number of stored indexed is egal to the max node detph)
		//		a. Make this node a leaf
		//		b. remove this node index from the index stack
		//		c. return this node index
		// 5. else
		//		a. re-arrange all the triangle according to an algorithm (centroid, median, surface area heuristic, ...)
		//		b. get the std::partition midpoint triangle index and add it to the stack of all the midpoints
		//		c. build the left node and add it to the current node 
		//		d. build the right node and add it to the current node 
		//		e. remove this midpoint and this index from the stack
		//		f. return this node index;


		m_indices.push((int)m_nodes.size());
		m_nodes.push_back(Node{});

		m_nodes[m_indices.top()].aabb = triangleAABB(m_triangles[first]);
		for (int i = first + 1; i < last; i++)
		{
			AABB triangle_aabb = triangleAABB(m_triangles[i]);
			m_nodes[m_indices.top()].aabb.min = glm::min(m_nodes[m_indices.top()].aabb.min, triangle_aabb.min);
			m_nodes[m_indices.top()].aabb.max = glm::max(m_nodes[m_indices.top()].aabb.max, triangle_aabb.max);
		}
		m_nodes[m_indices.top()].aabb.center = (m_nodes[m_indices.top()].aabb.max + m_nodes[m_indices.top()].aabb.min) / 2.0f;

		// See if this node is a leaf
		if ((last - first) <= m_leaf_max_size || m_indices.size() - 1 >= m_depth_max_size)
		{

#ifdef SATANIA_BVH_CLUSTER_COLORED

			glm::vec4 color(1.0f);
			color.r = (rand() % 16) / 16.0;
			color.b = (rand() % 16) / 16.0;
			color.g = (rand() % 16) / 16.0;

			for (size_t i = first; i < last; i++)
			{
				m_triangles[i].vertices[0].color = color;
				m_triangles[i].vertices[1].color = color;
				m_triangles[i].vertices[2].color = color;
			}
#endif

			m_nodes[m_indices.top()].leaf = first;
			m_nodes[m_indices.top()].leaf_elem = last - first;
			m_nodes[m_indices.top()].node_left = 0;
			m_nodes[m_indices.top()].node_right = 0;

			int index = m_indices.top();
			m_indices.pop();
			return index;
		}
		else
		{
			m_midpoints.push(std::distance(m_triangles.begin(), std::partition(m_triangles.begin() + first, m_triangles.begin() + last, [axis, this](Triangle& triangle)
				{
					AABB triangle_aabb = triangleAABB(triangle);
					return centroidLess(triangle_aabb, m_nodes[m_indices.top()].aabb, axis);
				})));

			/*if (m_nodes[m_indices.top()].aabb.center.x > m_nodes[m_indices.top()].aabb.center.y &&
				m_nodes[m_indices.top()].aabb.center.x > m_nodes[m_indices.top()].aabb.center.z) {
				axis = AXIS_X;
			}
			else if (m_nodes[m_indices.top()].aabb.center.y > m_nodes[m_indices.top()].aabb.center.x &&
				m_nodes[m_indices.top()].aabb.center.y > m_nodes[m_indices.top()].aabb.center.z) {
				axis = AXIS_Y;
			}
			else {
				axis = AXIS_Z;
			}*/

			axis = (Axis)((axis + 1) % AXIS_MAX);

			m_nodes[m_indices.top()].node_left = buildNode(first, m_midpoints.top(), axis);
			m_nodes[m_indices.top()].node_right = buildNode(m_midpoints.top(), last, axis);
			m_nodes[m_indices.top()].leaf_elem = 0;
			m_nodes[m_indices.top()].leaf = 0;

			m_midpoints.pop();
			int index = m_indices.top();
			m_indices.pop();
			return index;
		}
	}

	static AABB triangleAABB(const Triangle& triangle)
	{
		AABB aabb;
		aabb.min = triangle.vertices[0].position;
		aabb.max = triangle.vertices[0].position;

		for (int i = 1; i < 3; i++)
		{
			aabb.min = glm::min(aabb.min, glm::vec3(triangle.vertices[i].position));
			aabb.max = glm::max(aabb.max, glm::vec3(triangle.vertices[i].position));
		}

		aabb.center = (aabb.max + aabb.min) / 2.0f;
		return aabb;
	}

	static bool centroidLess(const AABB& a, const AABB& b, Axis axis)
	{
		switch (axis)
		{
		case AXIS_X:
			return a.center.x < b.center.x;
		case AXIS_Y:
			return a.center.y < b.center.y;
		case AXIS_Z:
			return a.center.z < b.center.z;

		default:
			return a.center.x < b.center.x;
		}
	}

	int                     m_leaf_max_size;
	int                     m_depth_max_size;
	int                     m_root_node;
	std::vector<AABB>       m_node_aabb;
	std::stack<int>         m_indices;
	std::stack<int>         m_midpoints;
};