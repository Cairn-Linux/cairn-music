#pragma once

#include <QString>

class CompositionModel;

class ProjectStore
{
public:
    static bool save(const QString &path, const CompositionModel &model);
    static bool load(const QString &path, CompositionModel &model);
};
