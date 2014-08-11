/******************************************************************************
* Copyright (c) 2014, Hobu Inc.
*
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following
* conditions are met:
*
*     * Redistributions of source code must retain the above copyright
*       notice, this list of conditions and the following disclaimer.
*     * Redistributions in binary form must reproduce the above copyright
*       notice, this list of conditions and the following disclaimer in
*       the documentation and/or other materials provided
*       with the distribution.
*     * Neither the name of Hobu, Inc. or Flaxen Geo Consulting nor the
*       names of its contributors may be used to endorse or promote
*       products derived from this software without specific prior
*       written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
* "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
* LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
* FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
* COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
* BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
* OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
* AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
* OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
* OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
* OF SUCH DAMAGE.
****************************************************************************/

#pragma once

#include <map>
#include <memory>
#include <vector>

#include "pdal/Dimension.hpp"
#include "pdal/Metadata.hpp"
#include "pdal/RawPtBuf.hpp"

namespace pdal
{

class PointBuffer;
class PointContext;

class DimInfo
{
   friend class PointContext;
public:
    DimInfo() : m_detail(Dimension::COUNT), m_nextFree(Dimension::PROPRIETARY)
        {}

    std::vector<Dimension::Detail> m_detail;
    Dimension::IdList m_used;
    std::map<std::string, Dimension::Id::Enum> m_propIds;
    int m_nextFree = Dimension::PROPRIETARY;
};
typedef std::shared_ptr<DimInfo> DimInfoPtr;

// This provides a context for processing a set of points and allows the library
// to be used to process multiple point sets simultaneously.
class PointContext
{
    friend class PointBuffer;
private:
    DimInfoPtr m_dims;
    // Provides storage for the point data.
    RawPtBufPtr m_ptBuf;
    // Metadata storage;
    MetadataPtr m_metadata;

public:
    PointContext() : m_dims(new DimInfo()), m_ptBuf(new RawPtBuf()),
        m_metadata(new Metadata)
    {}

    RawPtBuf *rawPtBuf() const
        { return m_ptBuf.get(); }
    MetadataNode metadata()
        { return m_metadata->getNode(); }
    SpatialReference spatialRef() const
    {
        MetadataNode m = m_metadata->m_private.findChild("spatialreference");
        SpatialReference sref;
        sref.setWKT(m.value());
        return sref;
    }
    void setSpatialRef(const SpatialReference& sref)
    {
        MetadataNode mp = m_metadata->m_private;
        mp.addOrUpdate("spatialreference", sref.getRawWKT());
    }

    void registerDims(std::vector<Dimension::Id::Enum> ids)
    {
        for (auto ii = ids.begin(); ii != ids.end(); ++ii)
            registerDim(*ii);
    }

    void registerDims(Dimension::Id::Enum *id)
    {
        while (*id != Dimension::Id::Unknown)
            registerDim(*id++);
    }

    void registerDim(Dimension::Id::Enum id)
    {
        // This is simple.  We don't try to deal with multiple people
        // registering dimensions with various sizes/types.
        if (m_dims->m_detail[id].type() != Dimension::Type::None)
            return;

        m_dims->m_used.push_back(id);
        m_dims->m_detail[id].m_type = Dimension::defaultType(id);
    }

    // The type and size are REQUESTS, not absolutes.  If someone else
    // has already registered with the same name, you get the existing
    // dimension size/type.
    Dimension::Id::Enum assignDim(const std::string& name,
        Dimension::Type::Enum type)
    {
        auto di = m_dims->m_propIds.find(name);
        if (di != m_dims->m_propIds.end())
            return di->second;

        //ABELL - Right now we don't upgrde dimensions (larger size/type)
        Dimension::Id::Enum id = (Dimension::Id::Enum)m_dims->m_nextFree++;
        m_dims->m_used.push_back(id);
        m_dims->m_detail[id].m_type = type;
        return id;
    }

    Dimension::Id::Enum registerOrAssignDim(const std::string name,
       Dimension::Type::Enum type)
    {
        Dimension::Id::Enum id = Dimension::id(name);
        if (id != Dimension::Id::Unknown)
        {
            registerDim(id);
            return id;
        }
        return assignDim(name, type);
    } 

    Dimension::Id::Enum findDim(const std::string& name)
    {
        Dimension::Id::Enum id = Dimension::id(name);
        if (id != Dimension::Id::Unknown)
            return id;
        auto di = m_dims->m_propIds.find(name);
        return (di != m_dims->m_propIds.end() ? di->second :
            Dimension::Id::Unknown);
    }

    std::string dimName(Dimension::Id::Enum id) const
    {
        std::string name = Dimension::name(id);
        if (!name.empty())
            return name;
        for (auto pi = m_dims->m_propIds.begin();
                pi != m_dims->m_propIds.end(); ++pi)
            if (pi->second == id)
                return pi->first;
        return "";
    }

    bool hasDim(Dimension::Id::Enum id) const
        { return m_dims->m_detail[id].m_type != Dimension::Type::None; }

    const Dimension::IdList& dims() const
        { return m_dims->m_used; }

private:
    Dimension::Detail *dimDetail(Dimension::Id::Enum id) const
        { return &(m_dims->m_detail[(size_t)id]); }
};

} //namespace
