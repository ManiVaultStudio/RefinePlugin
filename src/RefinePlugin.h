#pragma once

#include <Dataset.h>
#include <ViewPlugin.h>

#include <actions/DatasetPickerAction.h>
#include <widgets/DropWidget.h>

#include <PointData/PointData.h>

#include <QWidget>

using namespace mv::plugin;
using namespace mv::gui;

class QLabel;

class RefinePlugin : public ViewPlugin
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

    void loadData(const mv::Datasets& datasets);

    /**
     * Invoked when a data event occurs
     * @param dataEvent Data event which occurred
     */
    void onDataEvent(mv::DatasetEvent* dataEvent);

    ViewPlugin* getRefineScatterplot();

    void onRefine();

protected:
    mv::Dataset<Points>     _points;                    /** Points smart pointer */
    DatasetPickerAction     _datasetPickerAction;       /** list all current data sets */
};

/**
 * Plugin factory class
 *
 * Note: Factory does not need to be altered (merely responsible for generating new plugins when requested)
 */
class RefinePluginFactory : public ViewPluginFactory
{
    Q_INTERFACES(mv::plugin::ViewPluginFactory mv::plugin::PluginFactory)
    Q_OBJECT
    Q_PLUGIN_METADATA(IID   "studio.manivault.RefinePlugin"
                      FILE  "RefinePlugin.json")

public:

    /** Default constructor */
    RefinePluginFactory() {}

    /** Destructor */
    ~RefinePluginFactory() override {}
    
    /** Creates an instance of the example view plugin */
    ViewPlugin* produce() override;

    /** Returns the data types that are supported by the example view plugin */
    mv::DataTypes supportedDataTypes() const override;

    /**
     * Get plugin trigger actions given \p datasets
     * @param datasets Vector of input datasets
     * @return Vector of plugin trigger actions
     */
    PluginTriggerActions getPluginTriggerActions(const mv::Datasets& datasets) const override;
};
