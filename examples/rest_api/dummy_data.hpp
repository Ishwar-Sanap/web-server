#pragma once

#include <stdexcept>
#include <string>
#include <nlohmann/json.hpp>

namespace rest_api
{
    class DummyDataStore
    {
    public:
        using Json = nlohmann::json;

        DummyDataStore()
            : m_collections{
                  {"users",
                   Json::array({
                       {{"id", "1"}, {"name", "Ada Lovelace"}, {"email", "ada@example.com"}, {"active", true}},
                       {{"id", "2"}, {"name", "Grace Hopper"}, {"email", "grace@example.com"}, {"active", true}},
                       {{"id", "3"}, {"name", "Alan Turing"}, {"email", "alan@example.com"}, {"active", false}},
                   })},
                  {"posts",
                   Json::array({
                       {{"id", "1"}, {"userId", "1"}, {"title", "First post"}, {"published", true}},
                       {{"id", "2"}, {"userId", "2"}, {"title", "A second post"}, {"published", false}},
                       {{"id", "3"}, {"userId", "1"}, {"title", "Working with JSON"}, {"published", true}},
                   })}
              }
        {
        }

        const Json &list(const std::string &collection) const
        {
            return getCollection(collection);
        }

        Json *find(const std::string &collection, const std::string &id)
        {
            Json &items = getCollection(collection);
            for (Json &item : items)
            {
                if (item.value("id", std::string{}) == id)
                {
                    return &item;
                }
            }
            return nullptr;
        }

        Json create(const std::string &collection, Json item)
        {
            if (!item.is_object())
            {
                throw std::invalid_argument("A collection item must be a JSON object");
            }

            Json &items = getCollection(collection);
            std::size_t next_id = items.size() + 1;
            while (find(collection, std::to_string(next_id)) != nullptr)
            {
                ++next_id;
            }
            item["id"] = std::to_string(next_id);
            items.push_back(item);
            return item;
        }

        Json *replace(const std::string &collection, const std::string &id, Json item)
        {
            if (!item.is_object())
            {
                throw std::invalid_argument("A collection item must be a JSON object");
            }

            Json *existing = find(collection, id);
            if (existing == nullptr)
            {
                return nullptr;
            }

            item["id"] = id;
            *existing = std::move(item);
            return existing;
        }

        Json *update(const std::string &collection, const std::string &id, const Json &fields)
        {
            if (!fields.is_object())
            {
                throw std::invalid_argument("Update fields must be a JSON object");
            }

            Json *existing = find(collection, id);
            if (existing == nullptr)
            {
                return nullptr;
            }

            for (auto it = fields.begin(); it != fields.end(); ++it)
            {
                if (it.key() != "id")
                {
                    (*existing)[it.key()] = it.value();
                }
            }
            return existing;
        }

        bool erase(const std::string &collection, const std::string &id)
        {
            Json &items = getCollection(collection);
            for (auto it = items.begin(); it != items.end(); ++it)
            {
                if (it->value("id", std::string{}) == id)
                {
                    items.erase(it);
                    return true;
                }
            }
            return false;
        }

    private:
        Json m_collections;

        Json &getCollection(const std::string &collection)
        {
            auto it = m_collections.find(collection);
            if (it == m_collections.end() || !it->is_array())
            {
                throw std::out_of_range("Unknown dummy-data collection: " + collection);
            }
            return *it;
        }

        const Json &getCollection(const std::string &collection) const
        {
            auto it = m_collections.find(collection);
            if (it == m_collections.end() || !it->is_array())
            {
                throw std::out_of_range("Unknown dummy-data collection: " + collection);
            }
            return *it;
        }
    };
}
