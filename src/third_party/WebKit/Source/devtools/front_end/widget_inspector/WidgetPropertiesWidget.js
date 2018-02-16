/*
 * Copyright (C) 2017 Bloomberg Finance L.P.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS," WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

/**
 * @constructor
 * @param {!string}
 * @extends {WebInspector.ThrottledWidget}
 */
WebInspector.WidgetPropertiesWidget = function()
{
    WebInspector.ThrottledWidget.call(this);

    WebInspector.targetManager.addModelListener(WebInspector.DOMModel, WebInspector.DOMModel.Events.AttrModified, this._onNodeChange, this);
    WebInspector.targetManager.addModelListener(WebInspector.DOMModel, WebInspector.DOMModel.Events.AttrRemoved, this._onNodeChange, this);
    WebInspector.targetManager.addModelListener(WebInspector.DOMModel, WebInspector.DOMModel.Events.CharacterDataModified, this._onNodeChange, this);
    WebInspector.targetManager.addModelListener(WebInspector.DOMModel, WebInspector.DOMModel.Events.ChildNodeCountUpdated, this._onNodeChange, this);
    WebInspector.context.addFlavorChangeListener(WebInspector.DOMNode, this._setNode, this);
    WebInspector.targetManager.addModelListener(WebInspector.DOMModel, WebInspector.DOMModel.Events.NodeInspected, this._setNode, this);

    this.remoteObj;
    this.isUpdating = false;

    //WidgetInspector-specific
    //properties that have irregular getters/setters
    //sets value to null when not given a value instead of returning
    this.SKIPPED_PROPERTIES = { 
        child : 1,
        anchor : 2
    };
}

/**
 * @param {!string}
 * @return {!WebInspector.ElementsSidebarViewWrapperPane}
 */
WebInspector.WidgetPropertiesWidget.createSidebarWrapper = function()
{
    return new WebInspector.ElementsSidebarViewWrapperPane(WebInspector.UIString("Edit Properties"), new WebInspector.WidgetPropertiesWidget());
}

WebInspector.WidgetPropertiesWidget._objectGroupName = "properties-sidebar-pane";


/**
 * WidgetInspector-specific
 * @param {!Element} widgetElement
 * @param {!RemoteObject} property
 * @param {!string} value
 */
WebInspector.WidgetPropertiesWidget.setPropertyValue = function (widgetElement, property, value, callback) {
    var propName = property.value.split(" ")[0];

    var inspectorMap = WebInspector.WidgetInspectorPanel.instance()._widgetInspectorMap;
    inspectorMap.callFunction("function() { return this.editableLayoutProp('" + propName + "'); }", [{}],
        function (result, wasThrown) {
        var editableLayoutProp = result.description;
        if (editableLayoutProp !== "null") { // call StackPanel layout prop
            var functionString = "function() {  if(this.parentElement.widget)" +
            " { this.parentElement.widget." + editableLayoutProp + "(this, " + value + " );  } }";
            widgetElement.callFunction(functionString, [{}], callback);
            return;
        }

        var nameMap = WebInspector.WidgetInspectorPanel.instance()._nameMap;
        var functionString = "function() {  if(this." + nameMap.jsWidget + " && this." + nameMap.jsWidget + ".properties)" +
            " { this." + nameMap.jsWidget + ".properties." + propName + "(this, " + value + " );  } }";

        widgetElement.callFunction(functionString, [{}], callback);
    }.bind(this));
}

WebInspector.WidgetPropertiesWidget.prototype = {
    /**
     * @param {!WebInspector.Event} event
     */
    _setNode: function(event)
    {
        this._node = /** @type {?WebInspector.DOMNode} */(event.data);
        this.update();
    },


    /**
     * @override
     * @protected
     * @return {!Promise.<?>}
     */
    doUpdate: function()
    {
        var node = this._node;

        //if not a widget, don't update
        var symbols = Object.getOwnPropertySymbols(node);
        var treeElementWidg;
        //get treeElement of current node
        for (var i = 0; i < symbols.length; i++){
            var str = symbols[i].toString();
            if(str == "Symbol(WidgetElement)"){
                treeElementWidg = node[Object.getOwnPropertySymbols(node)[i]];
                break;
            }
        }
        if (!treeElementWidg || !treeElementWidg._listItemNode ||
            !treeElementWidg._listItemNode.className.includes("widget") ||
            treeElementWidg._listItemNode.className.includes("not-widget")) {
            return;
        }

        //TODO: find another way to handle being called multiple times
        if(this.isUpdating){
            return;
        }
        else {
            this.isUpdating = true;
        }

        if (this._lastRequestedNode) {
            this._lastRequestedNode.target().runtimeAgent().releaseObjectGroup(WebInspector.WidgetPropertiesWidget._objectGroupName);
            delete this._lastRequestedNode;
        }

        if (!this._node) {
            this.element.removeChildren();
            this.sections = [];
            return Promise.resolve();
        }


        this._lastRequestedNode = this._node;
        return this._node.resolveToObjectPromise(WebInspector.WidgetPropertiesWidget._objectGroupName)
            .then(nodeResolved.bind(this))

        /**
         * @param {?WebInspector.RemoteObject} object
         * @this {WebInspector.WidgetPropertiesWidget}
         */
        function nodeResolved(object)
        {
            this.remoteObj = object;

            if (!object)
                return;

            /**
             * @suppressReceiverCheck
             * @this {*}
             */
            function protoList()
            {
                var proto = this;
                var result = { __proto__: null };
                var counter = 1;
                while (proto) {
                    result[counter++] = proto;
                    proto = proto.__proto__;
                }
                return result;
            }
            var promise = object.callFunctionPromise(protoList).then(nodePrototypesReady.bind(this));
            object.release();
            return promise;
        }

        /**
         * @param {!{object: ?WebInspector.RemoteObject, wasThrown: (boolean|undefined)}} result
         * @this {WebInspector.WidgetPropertiesWidget}
         */
        function nodePrototypesReady(result)
        {
            if (!result.object || result.wasThrown)
                return;

            var promise = result.object.getOwnPropertiesPromise().then(fillSection.bind(this));
            result.object.release();
            return promise;
        }

        /**
         * @param {!{properties: ?Array.<!WebInspector.RemoteObjectProperty>, internalProperties: ?Array.<!WebInspector.RemoteObjectProperty>}} result
         * @this {WebInspector.WidgetPropertiesWidget}
         */
        function fillSection(result)
        {
            if (!result || !result.properties)
                return;

            var properties = result.properties;
            var expanded = [];
            var sections = this.sections || [];
            for (var i = 0; i < sections.length; ++i)
                expanded.push(sections[i].expanded);

            this.element.removeChildren();
            this.sections = [];

            var property;

            // Get array of property user-friendly names.
            for (var i = 0; i < properties.length; ++i) {
                if(properties[i].value._description.split(".").includes("widget")){
                    property = properties[i].value;
                    break;
                }
            }

            if(property){
                this.remoteObjMap = new Map(); /* maps old properties to new */
                this.remoteObjProperties = []; /* array of new properties */
                property.getAllProperties(false, this._propsCallback.bind(this));
            }
        }
    },

    /**
     * @param {!WebInspector.Event} event
     */
    _propertyExpanded: function(event)
    {
    },

    /**
     * WidgetInspector-specific
     * @param {?Array.<!WebInspector.RemoteObjectProperty>} properties
     * @param {?Array.<!WebInspector.RemoteObjectProperty>=} internalProperties
     * Traverse widget properties to find widget objects
     * Expand widget object
     */
    _propsCallback: function(properties, internalProperties) {
        var jsWidgetFound = false;
        var nameFound = false;
        var widgetFound = false;
        
        if (!properties) {
            return;
        }
        else {
            var nameMap = WebInspector.WidgetInspectorPanel.instance()._nameMap;
            for (var i = 0; i < properties.length; ++i) {
                if (properties[i].name == nameMap.jsWidget) {
                    this[nameMap.jsWidget] = properties[i].value;
                    jsWidgetFound = true;
                }
                else if (properties[i].name == "widget") {
                    properties[i].value.getAllProperties(false, this._domNodeCallback.bind(this));
                    widgetFound = true;
                }
                if (jsWidgetFound && widgetFound) {
                    break;
                }
            }
        }
    },

    /**
     * WidgetInspector-specific
     * Traverse widget properties to find p_domRoot and expand
     */
    _domNodeCallback: function(properties, internalProperties) {
        if (!properties) {
            return;
        }
        else {
            var nameMap = WebInspector.WidgetInspectorPanel.instance()._nameMap;
            for (var k = 0; k < properties.length; k++) {
                if (properties[k].name == "p_domRoot") {
                    this.widgetElement = properties[k].value;
                    this[nameMap.jsWidget].getAllProperties(false, this._propertiesCallback.bind(this));
                }
                else if (properties[k].name == "_klass") {
                    this._klass = properties[k].value._description;
                }
                else if (properties[k].name == "p_externalState") {
                    this._externalState = properties[k];
                }
            }
        }
    },

    /**
     * WidgetInspector-specific
     * Traverse p_domRoot properties and expand
     */
    _propertiesCallback: function(properties, internalProperties){
        if(!properties){
            return;
        }
        else{
            for(var j = 0; j < properties.length; j++){
                if(properties[j].name == "properties"){
                    this.propSection = properties[j].value;
                    properties[j].value.getAllProperties(false, this._valuesCallback.bind(this));
                    break;
                }
            }
        }
    },

    /**
     * WidgetInspector-specific
     * Begin process of iterating through properties by calling _getPropValue()
     */
    _valuesCallback: function(properties, internalProperties){
        if(!properties){ 
            return; 
        }
        else{
            this._getPropValue(0, properties);
        }
    },

    /**
     * WidgetInspector-specific
     * @param {number} index
     * @param {?Array.<!WebInspector.RemoteObjectProperty>} properties
     * Checks a single property
     * If the property is valid, send a function call to retrieve its value
     * If the index is out of bounds, create the sidebar panel and populate with all properties
     */
    _getPropValue: function(index, properties){
        this.index = index;
        this.properties = properties;

        //Index is out of bounds; all the properties have been processed
        //Make a new ObjectPropertiesSection instance with the new properties 
        if(index >= properties.length){

            // child layout properties (for widgets in layout containers)
            if (this._externalState) {
                this._externalState.name = "<Layout properties>";
                this._externalState.newProp = true;
                this._externalState.widgetElement = this.widgetElement;
                this.remoteObjProperties.push(this._externalState);
            }

            var section =  new WebInspector.ObjectPropertiesSection(this.propSection, "Edit Properties", "empty", false, this.remoteObjProperties, true);
            this.sections.push(section);
            section.expand();
            this.element.appendChild(section.element);
            section.addEventListener(TreeOutline.Events.ElementExpanded, this._propertyExpanded, this);
            //Signifies that sidebar is no longer currently updating
            this.isUpdating = false;
        }
        else{
            var propName = properties[index].name;

            //check if property getter is valid to run
            var inspectorMap = WebInspector.WidgetInspectorPanel.instance()._widgetInspectorMap;
            inspectorMap.callFunction("function() { return this.isSkippedProp('" + this._klass + "','" + propName + "'); }", [{}],
                function (result, wasThrown) {

                    var isSkippedProp = result.description === "true";
                    if (isSkippedProp || propName in this.SKIPPED_PROPERTIES) {
                        this._getPropValue(++this.index, this.properties);
                        return;
                    }

                    //Retrieve property value
                    if(!propName.includes("__")){
                        var nameMap = WebInspector.WidgetInspectorPanel.instance()._nameMap;
                        var functionString = "function() { if(this." + nameMap.jsWidget + " && this." + nameMap.jsWidget + ".properties) { return this." + nameMap.jsWidget + ".properties." + propName + "(this);  } else { return ''; } }";
                        if(this.widgetElement){
                            this.widgetElement.callFunction(functionString, [{}], this._remoteObjCallback.bind(this));
                        }
                        else {
                            this._getPropValue(++this.index, this.properties);
                        }
                    }
                    else{
                        this._getPropValue(++this.index, this.properties);
                    }
            }.bind(this));   
        }
    },

    /**
     * WidgetInspector-specific
     * @param {Object} result
     * @param {boolean} wasThrown
     * If value retrieval was successful, make a new RemoteObjectProperty instance with the correct values and add to
     * list of properties
     */
    _remoteObjCallback: function(result, wasThrown) {
        if(result == null || result._description.includes("TypeError")){
            console.error("Failed Result: ", result);
            
        }
        else if (this.index < this.properties.length){
            var propName = this.properties[this.index].name;
            //If property is already in map, it was already processed - skip
            if(this.remoteObjMap.get(propName)){
                console.log(propName + " was already processed");
                return;
            }
            else if(result._subtype != "error"){ 
                //if result is valid, then make a new RemoteObjectProperty instance with its values
                var currentProp = this.properties[this.index];
                var inspectorMap = WebInspector.WidgetInspectorPanel.instance()._widgetInspectorMap;
                inspectorMap.callFunction("function() { return this.clientPropertyToServerProperty('" + this._klass + "','" + currentProp.name + "'); }", [{}], function (serverProp, wasThrown) {

                    currentProp.clientProperty = currentProp.name; //save client-side name
                    currentProp.name = currentProp.name + " (" + serverProp.description + ")";
                    currentProp.value._description = result._description;
                    currentProp.value._type = result._type;
                    currentProp.value._subtype = result._subtype;
                    currentProp.newProp = true;
                    currentProp.widgetElement = this.widgetElement;
                    this.remoteObjMap.set(propName, result._description);
                    this.remoteObjProperties.push(currentProp);
                }.bind(this));
            }
        }
        //call next property
        this._getPropValue(++this.index, this.properties);
    },

    /**
     * @param {!WebInspector.Event} event
     */
    _propertyExpanded: function(event)
    {
        WebInspector.userMetrics.actionTaken(WebInspector.UserMetrics.Action.DOMPropertiesExpanded);
    },

    /**
     * @param {!WebInspector.Event} event
     */
    _onNodeChange: function(event)
    {
        if (!this._node)
            return;
        var data = event.data;
        var node = /** @type {!WebInspector.DOMNode} */ (data instanceof WebInspector.DOMNode ? data : data.node);
        if (this._node !== node)
            return;
        this.update();
    },

    __proto__: WebInspector.ThrottledWidget.prototype
}
