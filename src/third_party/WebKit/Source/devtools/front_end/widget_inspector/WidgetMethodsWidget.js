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
WebInspector.WidgetMethodsWidget = function()
{
    WebInspector.ThrottledWidget.call(this);

    WebInspector.targetManager.addModelListener(WebInspector.DOMModel, WebInspector.DOMModel.Events.AttrModified, this._onNodeChange, this);
    WebInspector.targetManager.addModelListener(WebInspector.DOMModel, WebInspector.DOMModel.Events.AttrRemoved, this._onNodeChange, this);
    WebInspector.targetManager.addModelListener(WebInspector.DOMModel, WebInspector.DOMModel.Events.CharacterDataModified, this._onNodeChange, this);
    WebInspector.targetManager.addModelListener(WebInspector.DOMModel, WebInspector.DOMModel.Events.ChildNodeCountUpdated, this._onNodeChange, this);
    WebInspector.context.addFlavorChangeListener(WebInspector.DOMNode, this._setNode, this);
    WebInspector.targetManager.addModelListener(WebInspector.DOMModel, WebInspector.DOMModel.Events.NodeInspected, this._setNode, this);

}

/**
 * @param {!string}
 * @return {!WebInspector.ElementsSidebarViewWrapperPane}
 */
WebInspector.WidgetMethodsWidget.createSidebarWrapper = function()
{
    return new WebInspector.ElementsSidebarViewWrapperPane(WebInspector.UIString("Call Methods"), new WebInspector.WidgetMethodsWidget());
}

WebInspector.WidgetMethodsWidget._objectGroupName = "properties-sidebar-pane";

WebInspector.WidgetMethodsWidget.prototype = {
    /**
     * @param {!WebInspector.Event} event
     */
    _setNode: function(event)
    {
        this._node = event.data;
        this.update();
    },


    /**
     * @override
     * @protected
     * @return {!Promise.<?>}
     */
    doUpdate: function()
    {
        if (this._lastRequestedNode) {
            this._lastRequestedNode.target().runtimeAgent().releaseObjectGroup(WebInspector.WidgetMethodsWidget._objectGroupName);
            delete this._lastRequestedNode;
        }

        if (!this._node) {
            this.element.removeChildren();
            this.sections = [];
            return Promise.resolve();
        }

        this._lastRequestedNode = this._node;
        return this._node.resolveToObjectPromise(WebInspector.WidgetMethodsWidget._objectGroupName)
            .then(nodeResolved.bind(this))

        /**
         * @param {?WebInspector.RemoteObject} object
         * @this {WebInspector.WidgetMethodsWidget}
         */
        function nodeResolved(object)
        {
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
         * @this {WebInspector.WidgetMethodsWidget}
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
         * @this {WebInspector.WidgetMethodsWidget}
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
                property.getAllProperties(false, this._propsCallback.bind(this));
            }
        }
    },

    /**
     * WidgetInspector-specific
     * @param {?Array.<!WebInspector.RemoteObjectProperty>} properties
     * @param {?Array.<!WebInspector.RemoteObjectProperty>=} internalProperties
     */
    //get widget object
    _propsCallback: function(properties, internalProperties){
        if (!properties) {
            return;
        }

        else {
            var namingMap = WebInspector.WidgetInspectorPanel.instance()._nameMap;
            for (var i = 0; i < properties.length; ++i) {
                if (properties[i].name == namingMap.jsWidget) {
                    this[namingMap.jsWidget] = properties[i].value;
                    properties[i].value.getAllProperties(false, this._propertiesCallback.bind(this));
                }
            }
        }
    },

    /**
     * WidgetInspector-specific
     * @param {?Array.<!WebInspector.RemoteObjectProperty>} properties
     * @param {?Array.<!WebInspector.RemoteObjectProperty>=} internalProperties
     */
    //get Properties and Methods objects
    _propertiesCallback: function(properties, internalProperties){
        if(!properties){
            return;
        }

        else{
            for(var j = 0; j < properties.length; j++){
                if(properties[j].name == "methods"){
                    var prop = properties[j].value;
                    var title = "Call Methods";
                    var section = new WebInspector.ObjectPropertiesSection(prop, title);
                    this.sections.push(section);
                    section.expand();
                    this.element.appendChild(section.element);
                }
            }
        }
    },

    /**
     * WidgetInspector-specific
     * @param {!WebInspector.Event} event
     */
    _propertyExpanded: function(event)
    {
        WebInspector.userMetrics.actionTaken(WebInspector.UserMetrics.Action.DOMPropertiesExpanded);
    },

    /**
     * WidgetInspector-specific
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


    //GETTERS/SETTERS=========================
    /**
    * @param {!WebInspector.DOMNode} node
    */
    setNodeExplicit: function(node){
        this._node = node;
    },

    getElement: function(){
        return this.element;
    },

    getRemoteObj: function(){
        return this.remoteObj;
    },

    getNode: function(){
        return this._node;
    },
    //====================================

    __proto__: WebInspector.ThrottledWidget.prototype
}
