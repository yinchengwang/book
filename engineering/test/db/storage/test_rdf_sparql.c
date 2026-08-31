/**
 * @file test_rdf_sparql.c
 * @brief SPARQL parser tests for FILTER, OPTIONAL, and GROUP BY
 */
#include <gtest/gtest.h>
#include <string.h>
#include <stdio.h>

#include "db/storage/rdf/sparql_parser.h"

class SparqlParserTest : public ::testing::Test {
protected:
    void SetUp() override {
    }

    void TearDown() override {
    }
};

/* ========================================================================
 * Basic Parsing Tests
 * ======================================================================== */

TEST_F(SparqlParserTest, ParseSimpleSelect) {
    sparql_query_t query;
    sparql_parse("SELECT ?x WHERE { ?x <http://example.org/name> \"John\" }", &query);

    EXPECT_EQ(query.type, SPARQL_QUERY_SELECT);
    EXPECT_EQ(query.variable_count, 1);
    EXPECT_STREQ(query.variables[0], "x");
    EXPECT_EQ(query.triple_pattern_count, 1);

    sparql_free(&query);
}

TEST_F(SparqlParserTest, ParseMultipleVariables) {
    sparql_query_t query;
    sparql_parse("SELECT ?x ?y ?z WHERE { ?x ?y ?z }", &query);

    EXPECT_EQ(query.variable_count, 3);
    EXPECT_STREQ(query.variables[0], "x");
    EXPECT_STREQ(query.variables[1], "y");
    EXPECT_STREQ(query.variables[2], "z");

    sparql_free(&query);
}

TEST_F(SparqlParserTest, ParseTriplePatterns) {
    sparql_query_t query;
    sparql_parse(
        "SELECT ?person WHERE {"
        "  ?person <http://xmlns.com/foaf/0.1/name> \"John\" ."
        "  ?person <http://xmlns.com/foaf/0.1/age> \"30\" ."
        "}",
        &query);

    EXPECT_EQ(query.triple_pattern_count, 2);
    EXPECT_TRUE(query.triple_patterns[0].is_subject_var);
    EXPECT_STREQ(query.triple_patterns[0].subject_var, "person");
    EXPECT_STREQ(query.triple_patterns[0].predicate_uri, "http://xmlns.com/foaf/0.1/name");
    EXPECT_STREQ(query.triple_patterns[0].object_literal, "John");

    sparql_free(&query);
}

TEST_F(SparqlParserTest, ParseLimitOffset) {
    sparql_query_t query;
    sparql_parse(
        "SELECT ?x WHERE { ?x ?y ?z } LIMIT 50 OFFSET 10",
        &query);

    EXPECT_EQ(query.limit, 50);
    EXPECT_EQ(query.offset, 10);

    sparql_free(&query);
}

TEST_F(SparqlParserTest, ParseRdfType) {
    sparql_query_t query;
    sparql_parse(
        "SELECT ?x WHERE { ?x a <http://example.org/Person> }",
        &query);

    EXPECT_EQ(query.triple_pattern_count, 1);
    EXPECT_STREQ(query.triple_patterns[0].predicate_uri,
                 "http://www.w3.org/1999/02/22-rdf-syntax-ns#type");

    sparql_free(&query);
}

TEST_F(SparqlParserTest, ParseQueryWithPrefix) {
    sparql_query_t query;
    sparql_parse(
        "PREFIX ex: <http://example.org/> "
        "SELECT ?x WHERE { ?x ex:name \"Test\" }",
        &query);

    EXPECT_EQ(query.prefix_count, 1);
    EXPECT_STREQ(query.prefixes[0].prefix, "ex:");
    EXPECT_STREQ(query.prefixes[0].uri, "http://example.org/");

    sparql_free(&query);
}

TEST_F(SparqlParserTest, ParseGroupBy) {
    sparql_query_t query;
    sparql_parse(
        "SELECT ?person WHERE {"
        "  ?person <http://example.org/type> \"Student\""
        "} GROUP BY ?person",
        &query);

    EXPECT_EQ(query.group_by_count, 1);
    EXPECT_STREQ(query.group_by_vars[0], "person");

    sparql_free(&query);
}

TEST_F(SparqlParserTest, ParseGroupByMultiple) {
    sparql_query_t query;
    sparql_parse(
        "SELECT ?dept ?year WHERE {"
        "  ?person <http://example.org/department> ?dept ."
        "  ?person <http://example.org/year> ?year"
        "} GROUP BY ?dept ?year",
        &query);

    EXPECT_EQ(query.group_by_count, 2);
    EXPECT_STREQ(query.group_by_vars[0], "dept");
    EXPECT_STREQ(query.group_by_vars[1], "year");

    sparql_free(&query);
}

TEST_F(SparqlParserTest, ParseOptionalPattern) {
    sparql_query_t query;
    sparql_parse(
        "SELECT ?person ?email WHERE {"
        "  ?person <http://xmlns.com/foaf/0.1/name> \"John\" ."
        "  OPTIONAL { ?person <http://xmlns.com/foaf/0.1/mbox> ?email }"
        "}",
        &query);

    EXPECT_EQ(query.triple_pattern_count, 1);
    EXPECT_NE(query.optionals, nullptr);
    EXPECT_EQ(query.optionals->pattern_count, 1);

    sparql_free(&query);
}

TEST_F(SparqlParserTest, ParseComplexQueryWithAllFeatures) {
    sparql_query_t query;
    sparql_parse(
        "PREFIX foaf: <http://xmlns.com/foaf/0.1/> "
        "SELECT ?person ?name WHERE {"
        "  ?person foaf:name ?name ."
        "} GROUP BY ?person ?name LIMIT 100",
        &query);

    EXPECT_EQ(query.type, SPARQL_QUERY_SELECT);
    EXPECT_EQ(query.variable_count, 2);
    EXPECT_EQ(query.triple_pattern_count, 1);
    EXPECT_EQ(query.group_by_count, 2);
    EXPECT_EQ(query.limit, 100);
    EXPECT_EQ(query.prefix_count, 1);

    sparql_free(&query);
}

/* ========================================================================
 * FILTER Evaluation Tests (Standalone)
 * ======================================================================== */

TEST_F(SparqlParserTest, EvaluateFilterNumericGreater) {
    sparql_binding_t bindings[] = {
        {"age", "35", 1},
    };

    sparql_filter_t filter;
    memset(&filter, 0, sizeof(filter));
    filter.type = SPARQL_EXPR_COMPARE;
    filter.op = SPARQL_OP_GT;
    strcpy(filter.var_left, "age");
    filter.value.int_val = 30;
    filter.value_type = 0;

    EXPECT_EQ(sparql_filter_evaluate(&filter, bindings, 1), 1);
}

TEST_F(SparqlParserTest, EvaluateFilterNumericLess) {
    sparql_binding_t bindings[] = {
        {"score", "50", 1},
    };

    sparql_filter_t filter;
    memset(&filter, 0, sizeof(filter));
    filter.type = SPARQL_EXPR_COMPARE;
    filter.op = SPARQL_OP_LT;
    strcpy(filter.var_left, "score");
    filter.value.int_val = 100;
    filter.value_type = 0;

    EXPECT_EQ(sparql_filter_evaluate(&filter, bindings, 1), 1);
}

TEST_F(SparqlParserTest, EvaluateFilterUnboundVariable) {
    sparql_binding_t bindings[] = {
        {"name", "John", 1},
    };

    sparql_filter_t filter;
    memset(&filter, 0, sizeof(filter));
    filter.type = SPARQL_EXPR_COMPARE;
    filter.op = SPARQL_OP_GT;
    strcpy(filter.var_left, "age");
    filter.value.int_val = 30;
    filter.value_type = 0;

    /* age is not bound, filter should fail */
    EXPECT_EQ(sparql_filter_evaluate(&filter, bindings, 1), 0);
}

TEST_F(SparqlParserTest, EvaluateFilterBoundFunction) {
    sparql_binding_t bindings[] = {
        {"email", "john@example.com", 1},
    };

    sparql_filter_t filter;
    memset(&filter, 0, sizeof(filter));
    filter.type = SPARQL_EXPR_FUNCTION;
    strcpy(filter.var_left, "email");

    EXPECT_EQ(sparql_filter_evaluate(&filter, bindings, 1), 1);

    /* Test unbound */
    sparql_binding_t unbound[] = {
        {"email", "", 0},
    };
    EXPECT_EQ(sparql_filter_evaluate(&filter, unbound, 1), 0);
}

TEST_F(SparqlParserTest, EvaluateFilterNotBound) {
    sparql_binding_t bindings[] = {
        {"email", "", 0},  /* unbound */
    };

    sparql_filter_t filter;
    memset(&filter, 0, sizeof(filter));
    filter.type = SPARQL_EXPR_FUNCTION;
    filter.op = SPARQL_OP_NE;  /* NOT BOUND */
    strcpy(filter.var_left, "email");

    EXPECT_EQ(sparql_filter_evaluate(&filter, bindings, 1), 1);

    /* Test bound */
    sparql_binding_t bound[] = {
        {"email", "john@example.com", 1},
    };
    EXPECT_EQ(sparql_filter_evaluate(&filter, bound, 1), 0);
}
