#pragma once

#include <Dataset.h>
#include <ViewPlugin.h>

#include <actions/DatasetPickerAction.h>
#include <actions/OptionAction.h>
#include <actions/ToggleAction.h>
#include <actions/TriggerAction.h>

#include <PointData/PointData.h>

#include <vector>

#include <QStringList>

class RefinePlugin : public mv::plugin::ViewPlugin
{
    Q_OBJECT

public:

    /**
     * Constructor
     * @param factory Pointer to the plugin factory
     */
    RefinePlugin(const PluginFactory* factory);

    /** Destructor */
    ~RefinePlugin() override = default;
    
    /** This function is called by the core after the view plugin has been created */
    void init() override;

public: // Functionality

    void loadData(const mv::Datasets& datasets);

    void onDataEvent(mv::DatasetEvent* dataEvent);

    void onRefine();

public: // Getter

    std::vector<mv::plugin::Plugin*> getOpenScatterplots();

    QStringList getScatterplotOptions();

public: // Serialization

    void fromVariantMap(const QVariantMap& variantMap) override;
    QVariantMap toVariantMap() const override;

private:
    mv::Dataset<Points>              _hsnePoints;                /** Current hsne points smart pointer */
    ViewPlugin*                      _scatterplotView;           /** Scatterplot to show refined scale in */

    mv::gui::TriggerAction           _refineAction;              /** big refine button */
    mv::gui::DatasetPickerAction     _datasetPickerAction;       /** list all current data sets */
    mv::gui::ToggleAction            _updateDatasetAction;       /** focus refine button on newly refined embedding */
    mv::gui::OptionAction            _scatterplotAction;         /** add new embedding as a tab in a new scatterplot */
};

// =============================================================================
// Factory
// =============================================================================

class RefinePluginFactory : public mv::plugin::ViewPluginFactory
{
    Q_INTERFACES(mv::plugin::ViewPluginFactory mv::plugin::PluginFactory)
    Q_OBJECT
    Q_PLUGIN_METADATA(IID   "studio.manivault.RefinePlugin"
                      FILE  "PluginInfo.json")

public:

    /** Constructor */
    RefinePluginFactory();
    
    /** Creates an instance of the example view plugin */
    ViewPlugin* produce() override;

    /** Returns the data types that are supported by the example view plugin */
    mv::DataTypes supportedDataTypes() const override;

    /**
     * Get plugin trigger actions given \p datasets
     * @param datasets Vector of input datasets
     * @return Vector of plugin trigger actions
     */
    mv::gui::PluginTriggerActions getPluginTriggerActions(const mv::Datasets& datasets) const override;
};
