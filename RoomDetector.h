#pragma once

#include "Room.h"
#include <unordered_set>
#include <sstream>
#include <algorithm>
#include <functional>

namespace winrt::estimate1
{
    class RoomDetector
    {
    public:
        static std::vector<std::shared_ptr<Room>> DetectRooms(const std::vector<std::unique_ptr<Wall>>& walls)
        {
            std::vector<std::shared_ptr<Room>> rooms;
            if (walls.size() < 3)
                return rooms;

            const double mergeTolerance = 5.0; // ��

            struct Node
            {
                WorldPoint pt;
            };

            std::vector<Node> nodes;
            auto findOrCreateNode = [&](const WorldPoint& p) -> int
            {
                for (int i = 0; i < static_cast<int>(nodes.size()); ++i)
                {
                    if (nodes[i].pt.Distance(p) <= mergeTolerance)
                        return i;
                }
                nodes.push_back(Node{ p });
                return static_cast<int>(nodes.size() - 1);
            };

            // ������ ����
            std::vector<std::vector<int>> adj;
            auto ensureAdjSize = [&](int idx)
            {
                if (idx >= static_cast<int>(adj.size()))
                    adj.resize(idx + 1);
            };

            for (const auto& wall : walls)
            {
                if (!wall) continue;
                int a = findOrCreateNode(wall->GetStartPoint());
                int b = findOrCreateNode(wall->GetEndPoint());
                if (a == b) continue;
                ensureAdjSize((std::max)(a, b));

                auto pushUnique = [](std::vector<int>& vec, int v)
                {
                    if (std::find(vec.begin(), vec.end(), v) == vec.end())
                        vec.push_back(v);
                };

                pushUnique(adj[a], b);
                pushUnique(adj[b], a);
            }

            std::vector<WorldPoint> nodePoints;
            nodePoints.reserve(nodes.size());
            for (const auto& n : nodes)
            {
                nodePoints.push_back(n.pt);
            }

            // ����� ������ ������� DFS
            const int maxDepth = 12;
            std::unordered_set<std::wstring> uniqueCycles;
            std::vector<int> path;

            std::function<void(int, int, int)> dfs = [&](int start, int current, int depth)
            {
                if (depth > maxDepth)
                    return;

                for (int next : adj[current])
                {
                    if (next == start && path.size() >= 3)
                    {
                        // �������� ����
                        std::vector<int> cycle = path;
                        AddCycle(cycle, nodePoints, uniqueCycles, rooms);
                        continue;
                    }

                    if (std::find(path.begin(), path.end(), next) != path.end())
                        continue; // �������� �������� � ����

                    path.push_back(next);
                    dfs(start, next, depth + 1);
                    path.pop_back();
                }
            };

            for (int i = 0; i < static_cast<int>(nodes.size()); ++i)
            {
                path.clear();
                path.push_back(i);
                dfs(i, i, 0);
            }

            return rooms;
        }

    private:
        static void AddCycle(const std::vector<int>& cycleNodes,
            const std::vector<WorldPoint>& points,
            std::unordered_set<std::wstring>& uniqueCycles,
            std::vector<std::shared_ptr<Room>>& rooms)
        {
            if (cycleNodes.size() < 3)
                return;

            // ������������ ������������� ��� ������������
            std::vector<int> normalized = cycleNodes;
            // ����� ����������� ������
            auto minIt = std::min_element(normalized.begin(), normalized.end());
            size_t minPos = static_cast<size_t>(std::distance(normalized.begin(), minIt));

            auto rotateToStart = [&](const std::vector<int>& src, size_t start) {
                std::vector<int> res;
                res.reserve(src.size());
                for (size_t i = 0; i < src.size(); ++i)
                {
                    res.push_back(src[(start + i) % src.size()]);
                }
                return res;
            };

            auto forward = rotateToStart(normalized, minPos);
            auto reversed = forward;
            std::reverse(reversed.begin(), reversed.end());
            auto canonicalVec = (forward < reversed) ? forward : reversed;

            std::wstringstream ss;
            for (int v : canonicalVec)
            {
                ss << v << L"-";
            }
            auto key = ss.str();
            if (uniqueCycles.find(key) != uniqueCycles.end())
                return;

            uniqueCycles.insert(key);

            // ��������� ������
            std::vector<WorldPoint> contour;
            contour.reserve(canonicalVec.size());
            for (int idx : canonicalVec)
            {
                contour.push_back(points[idx]);
            }

            // ��������� �������
            double area = Room::ComputeArea(contour);
            if (std::abs(area) < 1000.0) // ����� 1 ��.� � ����������
                return;

            auto room = std::make_shared<Room>();
            room->SetContour(contour);
            room->SetName(L"���������");
            room->SetNumber(std::to_wstring(rooms.size() + 1));
            rooms.push_back(room);
        }
    };
}
