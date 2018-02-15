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
 * @param {!WebInspector.ElementsTreeOutline} treeOutline
 */
WebInspector.WidgetTreeElementHighlighter = function(treeOutline)
{
    this._throttler = new WebInspector.Throttler(100);
    this._treeOutline = treeOutline;
    this._treeOutline.addEventListener(TreeOutline.Events.ElementExpanded, this._clearState, this);
    this._treeOutline.addEventListener(TreeOutline.Events.ElementCollapsed, this._clearState, this);
    this._treeOutline.addEventListener(WebInspector.WidgetTreeOutline.Events.SelectedNodeChanged, this._clearState, this);
    WebInspector.targetManager.addModelListener(WebInspector.DOMModel, WebInspector.DOMModel.Events.NodeHighlightedInOverlay, this._highlightNode, this);
    this._treeOutline.domModel().addEventListener(WebInspector.DOMModel.Events.InspectModeWillBeToggled, this._clearState, this);
}

WebInspector.WidgetTreeElementHighlighter.prototype = {
    /**
     * @param {!WebInspector.Event} event
     */
    _highlightNode: function(event)
    {
        if (!WebInspector.moduleSetting("highlightNodeOnHoverInOverlay").get())
            return;

        var domNode = /** @type {!WebInspector.DOMNode} */ (event.data);

        this._throttler.schedule(callback.bind(this));
        this._pendingHighlightNode = this._treeOutline.domModel() === domNode.domModel() ? domNode : null;

        /**
         * @this {WebInspector.WidgetTreeElementHighlighter}
         */
        function callback()
        {
            this._highlightNodeInternal(this._pendingHighlightNode);
            delete this._pendingHighlightNode;
            return Promise.resolve();
        }
    },

    /**
     * @param {?WebInspector.DOMNode} node
     */
    _highlightNodeInternal: function(node)
    {
        this._isModifyingTreeOutline = true;
        var treeElement = null;

        if (this._currentHighlightedElement) {
            var currentTreeElement = this._currentHighlightedElement;
            while (currentTreeElement !== this._alreadyExpandedParentElement) {
                if (currentTreeElement.expanded)
                    currentTreeElement.collapse();

                currentTreeElement = currentTreeElement.parent;
            }
        }

        delete this._currentHighlightedElement;
        delete this._alreadyExpandedParentElement;
        if (node) {
            var deepestExpandedParent = node;
            var treeElementSymbol = this._treeOutline.treeElementSymbol();
            while (deepestExpandedParent && (!deepestExpandedParent[treeElementSymbol] || !deepestExpandedParent[treeElementSymbol].expanded))
                deepestExpandedParent = deepestExpandedParent.parentNode;

            this._alreadyExpandedParentElement = deepestExpandedParent ? deepestExpandedParent[treeElementSymbol] : this._treeOutline.rootElement();
            treeElement = this._treeOutline.createTreeElementFor(node);

            // get the widget for this treeElement
            if (this._alreadyExpandedParentElement && this._alreadyExpandedParentElement._node && !this._alreadyExpandedParentElement._node._isWidget) {
                while (this._alreadyExpandedParentElement) {
                    this._alreadyExpandedParentElement = this._alreadyExpandedParentElement.parent;
                    if (this._alreadyExpandedParentElement && this._alreadyExpandedParentElement._node && this._alreadyExpandedParentElement._node._isWidget) {
                        break;
                    }
                }
            }
            if (treeElement && treeElement._node && !treeElement._node._isWidget) {
                while (treeElement) {
                    treeElement = treeElement.parent;
                    if (treeElement && treeElement._node && treeElement._node._isWidget) {
                        break;
                    }
                }
            }
        }

        this._currentHighlightedElement = treeElement;
        this._treeOutline.setHoverEffect(treeElement);
        if (treeElement)
            treeElement.reveal();

        this._isModifyingTreeOutline = false;
    },

    _clearState: function()
    {
        if (this._isModifyingTreeOutline)
            return;

        delete this._currentHighlightedElement;
        delete this._alreadyExpandedParentElement;
        delete this._pendingHighlightNode;
    }

}
