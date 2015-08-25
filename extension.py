#! /usr/bin/env python3
# -*- coding: utf-8 -*-
# vim:fenc=utf-8
#
# Copyright © 2015 Adrian Perez <aperez@igalia.com>
#
# Distributed under terms of the MIT license.

def on_document_loaded(webpage):
    # As en example, use the DOM to print the document title
    print("document-loaded: uri =", webpage.get_uri())
    document = webpage.get_dom_document()
    print("document-loaded: title =", document.get_title())


def on_page_created(extension, webpage):
    print("page-created: extension =", extension)
    print("page-created: page =", webpage)
    print("page-created: uri =", webpage.get_uri())
    webpage.connect("document-loaded", on_document_loaded)


def initialize(extension, arguments):
    print("initialize: extension =", extension)
    print("initialize: arguments =", arguments)
    extension.connect("page-created", on_page_created)
