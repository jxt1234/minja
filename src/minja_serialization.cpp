// src/minja_serialization.cpp
#include "minja/minja.hpp"
// The generated header should be available via include paths
#include "minja_schema_generated.h"
#include <stdexcept> // For std::runtime_error

namespace minja {

// --- Location ---
flatbuffers::Offset<minja::fbs::Location> Location::serialize(flatbuffers::FlatBufferBuilder& builder) const {
    auto source_str = this->source ? builder.CreateString(*this->source) : builder.CreateString("");
    return minja::fbs::CreateLocation(builder, source_str, this->pos);
}

// --- minja::json ---
flatbuffers::Offset<minja::fbs::JsonValue> json::serialize(flatbuffers::FlatBufferBuilder& builder) const {
    minja::fbs::JsonType fb_type = minja::fbs::JsonType::JSON_NULL;
    flatbuffers::Offset<flatbuffers::String> fb_string;
    int64_t fb_int = 0;
    double fb_double = 0.0;
    bool fb_bool = false;

    switch (this->mType) {
        case minja::ObjectType::JSON_INT64:
            fb_type = minja::fbs::JsonType::JSON_INT64;
            fb_int = this->mInt;
            break;
        case minja::ObjectType::JSON_DOUBLE:
            fb_type = minja::fbs::JsonType::JSON_DOUBLE;
            fb_double = this->mDouble;
            break;
        case minja::ObjectType::JSON_STRING:
            fb_type = minja::fbs::JsonType::JSON_STRING;
            if (!this->mString.empty()) fb_string = builder.CreateString(this->mString);
            break;
        case minja::ObjectType::JSON_BOOL:
            fb_type = minja::fbs::JsonType::JSON_BOOL;
            fb_bool = this->mBool;
            break;
        case minja::ObjectType::JSON_NULL:
        default:
            fb_type = minja::fbs::JsonType::JSON_NULL;
            break;
    }

    minja::fbs::JsonValueBuilder jvb(builder);
    jvb.add_mType(fb_type);
    if (!fb_string.IsNull()) { 
        jvb.add_mString(fb_string);
    }
    jvb.add_mInt(fb_int);
    jvb.add_mDouble(fb_double);
    jvb.add_mBool(fb_bool);
    return jvb.Finish();
}

// --- Value ---
flatbuffers::Offset<minja::fbs::Value> Value::serialize(flatbuffers::FlatBufferBuilder& builder) const {
    minja::fbs::MinjaValueType fb_value_type = minja::fbs::MinjaValueType::Primitive; // Default
    flatbuffers::Offset<minja::fbs::JsonValue> fb_primitive;
    flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<minja::fbs::Value>>> fb_array_values;
    flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<flatbuffers::String>>> fb_object_keys;
    flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<minja::fbs::Value>>> fb_object_values;

    if (this->is_primitive()) {
        fb_value_type = minja::fbs::MinjaValueType::Primitive;
        fb_primitive = this->primitive_.serialize(builder);
    } else if (this->is_array()) {
        fb_value_type = minja::fbs::MinjaValueType::Array;
        if (this->array_ && !this->array_->empty()) {
            std::vector<flatbuffers::Offset<minja::fbs::Value>> values_vec;
            values_vec.reserve(this->array_->size());
            for (const auto& val : *this->array_) {
                values_vec.push_back(val.serialize(builder));
            }
            if (!values_vec.empty()) fb_array_values = builder.CreateVector(values_vec);
        }
    } else if (this->is_object()) {
        fb_value_type = minja::fbs::MinjaValueType::Object;
        if (this->object_ && !this->object_->empty()) {
            std::vector<flatbuffers::Offset<flatbuffers::String>> keys_vec;
            std::vector<flatbuffers::Offset<minja::fbs::Value>> values_vec;
            keys_vec.reserve(this->object_->size());
            values_vec.reserve(this->object_->size());
            for (const auto& pair : *this->object_) {
                keys_vec.push_back(builder.CreateString(pair.first));
                values_vec.push_back(pair.second.serialize(builder));
            }
            if (!keys_vec.empty()) fb_object_keys = builder.CreateVector(keys_vec);
            if (!values_vec.empty()) fb_object_values = builder.CreateVector(values_vec);
        }
    }
    // Skipping callable_ as it's not directly serializable

    minja::fbs::ValueBuilder vb(builder);
    vb.add_value_type(fb_value_type);
    if (!fb_primitive.IsNull()) {
        vb.add_primitive(fb_primitive);
    }
    if (!fb_array_values.IsNull()) {
        vb.add_array_values(fb_array_values);
    }
    if (!fb_object_keys.IsNull()) {
        vb.add_object_keys(fb_object_keys);
    }
    if (!fb_object_values.IsNull()) {
        vb.add_object_values(fb_object_values);
    }
    return vb.Finish();
}

// --- ArgumentsExpression ---
flatbuffers::Offset<minja::fbs::ArgumentsExpressionData> ArgumentsExpression::serialize(flatbuffers::FlatBufferBuilder& builder) const {
    flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<minja::fbs::Expression>>> fb_args;
    if (!this->args.empty()) {
        std::vector<flatbuffers::Offset<minja::fbs::Expression>> args_vec;
        args_vec.reserve(this->args.size());
        for (const auto& arg_expr : this->args) {
            if (arg_expr) { 
                args_vec.push_back(arg_expr->serialize(builder));
            }
        }
        if (!args_vec.empty()) {
            fb_args = builder.CreateVector(args_vec);
        }
    }

    flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<flatbuffers::String>>> fb_kwarg_names;
    flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<minja::fbs::Expression>>> fb_kwarg_values;

    if (!this->kwargs.empty()) {
        std::vector<flatbuffers::Offset<flatbuffers::String>> kwarg_names_vec;
        std::vector<flatbuffers::Offset<minja::fbs::Expression>> kwarg_values_vec;
        kwarg_names_vec.reserve(this->kwargs.size());
        kwarg_values_vec.reserve(this->kwargs.size());

        for (const auto& kwarg_pair : this->kwargs) {
            kwarg_names_vec.push_back(builder.CreateString(kwarg_pair.first));
            if (kwarg_pair.second) { 
                kwarg_values_vec.push_back(kwarg_pair.second->serialize(builder));
            }
        }
        if (!kwarg_names_vec.empty()) {
            fb_kwarg_names = builder.CreateVector(kwarg_names_vec);
        }
        if (!kwarg_values_vec.empty()) {
           fb_kwarg_values = builder.CreateVector(kwarg_values_vec);
        }
    }
    
    minja::fbs::ArgumentsExpressionDataBuilder aedb(builder);
    if (!fb_args.IsNull()) {
        aedb.add_args(fb_args);
    }
    if (!fb_kwarg_names.IsNull()) {
        aedb.add_kwarg_names(fb_kwarg_names);
    }
    if (!fb_kwarg_values.IsNull()) {
        aedb.add_kwarg_values(fb_kwarg_values);
    }
    return aedb.Finish();
}


// --- Expression Subclasses ---

flatbuffers::Offset<minja::fbs::Expression> VariableExpr::serialize(flatbuffers::FlatBufferBuilder& builder) const {
    auto loc_offset = location.serialize(builder);
    auto name_offset = builder.CreateString(name);
    auto var_data_offset = minja::fbs::CreateVariableExprData(builder, name_offset);

    minja::fbs::ExpressionBuilder eb(builder);
    eb.add_location(loc_offset);
    eb.add_expression_type(minja::fbs::ExpressionType::Type_Variable);
    eb.add_expression_data_type(minja::fbs::ExpressionData::VariableExprData);
    eb.add_expression_data(var_data_offset.Union());
    return eb.Finish();
}

flatbuffers::Offset<minja::fbs::Expression> LiteralExpr::serialize(flatbuffers::FlatBufferBuilder& builder) const {
    auto loc_offset = location.serialize(builder);
    auto val_offset = value.serialize(builder);
    auto lit_data_offset = minja::fbs::CreateLiteralExprData(builder, val_offset);

    minja::fbs::ExpressionBuilder eb(builder);
    eb.add_location(loc_offset);
    eb.add_expression_type(minja::fbs::ExpressionType::Type_Liter);
    eb.add_expression_data_type(minja::fbs::ExpressionData::LiteralExprData);
    eb.add_expression_data(lit_data_offset.Union());
    return eb.Finish();
}

flatbuffers::Offset<minja::fbs::Expression> ArrayExpr::serialize(flatbuffers::FlatBufferBuilder& builder) const {
    auto loc_offset = location.serialize(builder);
    flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<minja::fbs::Expression>>> elements_offset;
    if (!elements.empty()) {
        std::vector<flatbuffers::Offset<minja::fbs::Expression>> elem_vec;
        elem_vec.reserve(elements.size());
        for (const auto& el : elements) {
            if(el) elem_vec.push_back(el->serialize(builder));
        }
        if(!elem_vec.empty()) elements_offset = builder.CreateVector(elem_vec);
    }
    auto arr_data_offset = minja::fbs::CreateArrayExprData(builder, elements_offset);

    minja::fbs::ExpressionBuilder eb(builder);
    eb.add_location(loc_offset);
    eb.add_expression_type(minja::fbs::ExpressionType::Type_Array);
    eb.add_expression_data_type(minja::fbs::ExpressionData::ArrayExprData);
    eb.add_expression_data(arr_data_offset.Union());
    return eb.Finish();
}

flatbuffers::Offset<minja::fbs::Expression> DictExpr::serialize(flatbuffers::FlatBufferBuilder& builder) const {
    auto loc_offset = location.serialize(builder);
    flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<minja::fbs::DictExprItemData>>> items_offset;
    if (!elements.empty()) {
        std::vector<flatbuffers::Offset<minja::fbs::DictExprItemData>> items_vec;
        items_vec.reserve(elements.size());
        for (const auto& pair : elements) {
            if (pair.first && pair.second) {
                auto key_off = pair.first->serialize(builder);
                auto val_off = pair.second->serialize(builder);
                items_vec.push_back(minja::fbs::CreateDictExprItemData(builder, key_off, val_off));
            }
        }
        if(!items_vec.empty()) items_offset = builder.CreateVector(items_vec);
    }
    auto dict_data_offset = minja::fbs::CreateDictExprData(builder, items_offset);

    minja::fbs::ExpressionBuilder eb(builder);
    eb.add_location(loc_offset);
    eb.add_expression_type(minja::fbs::ExpressionType::Type_Dict);
    eb.add_expression_data_type(minja::fbs::ExpressionData::DictExprData);
    eb.add_expression_data(dict_data_offset.Union());
    return eb.Finish();
}

flatbuffers::Offset<minja::fbs::Expression> SliceExpr::serialize(flatbuffers::FlatBufferBuilder& builder) const {
    auto loc_offset = location.serialize(builder);
    auto start_offset = start ? start->serialize(builder) : flatbuffers::Offset<minja::fbs::Expression>();
    auto end_offset = end ? end->serialize(builder) : flatbuffers::Offset<minja::fbs::Expression>();
    auto step_offset = step ? step->serialize(builder) : flatbuffers::Offset<minja::fbs::Expression>();
    auto slice_data_offset = minja::fbs::CreateSliceExprData(builder, start_offset, end_offset, step_offset);

    minja::fbs::ExpressionBuilder eb(builder);
    eb.add_location(loc_offset);
    eb.add_expression_type(minja::fbs::ExpressionType::Type_Slice);
    eb.add_expression_data_type(minja::fbs::ExpressionData::SliceExprData);
    eb.add_expression_data(slice_data_offset.Union());
    return eb.Finish();
}

flatbuffers::Offset<minja::fbs::Expression> SubscriptExpr::serialize(flatbuffers::FlatBufferBuilder& builder) const {
    auto loc_offset = location.serialize(builder);
    auto base_offset = base ? base->serialize(builder) : flatbuffers::Offset<minja::fbs::Expression>();
    auto index_offset = index ? index->serialize(builder) : flatbuffers::Offset<minja::fbs::Expression>();
    auto sub_data_offset = minja::fbs::CreateSubscriptExprData(builder, base_offset, index_offset);

    minja::fbs::ExpressionBuilder eb(builder);
    eb.add_location(loc_offset);
    eb.add_expression_type(minja::fbs::ExpressionType::Type_Subscript);
    eb.add_expression_data_type(minja::fbs::ExpressionData::SubscriptExprData);
    eb.add_expression_data(sub_data_offset.Union());
    return eb.Finish();
}

flatbuffers::Offset<minja::fbs::Expression> UnaryOpExpr::serialize(flatbuffers::FlatBufferBuilder& builder) const {
    auto loc_offset = location.serialize(builder);
    auto expr_offset = expr ? expr->serialize(builder) : flatbuffers::Offset<minja::fbs::Expression>();
    minja::fbs::UnaryOpType fb_op;
    switch (op) {
        case Op::Plus: fb_op = minja::fbs::UnaryOpType::Plus; break;
        case Op::Minus: fb_op = minja::fbs::UnaryOpType::Minus; break;
        case Op::LogicalNot: fb_op = minja::fbs::UnaryOpType::LogicalNot; break;
        case Op::Expansion: fb_op = minja::fbs::UnaryOpType::Expansion; break;
        case Op::ExpansionDict: fb_op = minja::fbs::UnaryOpType::ExpansionDict; break;
        default: throw std::runtime_error("Unknown UnaryOpType"); 
    }
    auto unary_data_offset = minja::fbs::CreateUnaryOpExprData(builder, expr_offset, fb_op);

    minja::fbs::ExpressionBuilder eb(builder);
    eb.add_location(loc_offset);
    eb.add_expression_type(minja::fbs::ExpressionType::Type_Unary);
    eb.add_expression_data_type(minja::fbs::ExpressionData::UnaryOpExprData);
    eb.add_expression_data(unary_data_offset.Union());
    return eb.Finish();
}

flatbuffers::Offset<minja::fbs::Expression> BinaryOpExpr::serialize(flatbuffers::FlatBufferBuilder& builder) const {
    auto loc_offset = location.serialize(builder);
    auto left_offset = left ? left->serialize(builder) : flatbuffers::Offset<minja::fbs::Expression>();
    auto right_offset = right ? right->serialize(builder) : flatbuffers::Offset<minja::fbs::Expression>();
    minja::fbs::BinaryOpType fb_op;
    switch (op) {
        case Op::StrConcat: fb_op = minja::fbs::BinaryOpType::StrConcat; break;
        case Op::Add: fb_op = minja::fbs::BinaryOpType::Add; break;
        case Op::Sub: fb_op = minja::fbs::BinaryOpType::Sub; break;
        case Op::Mul: fb_op = minja::fbs::BinaryOpType::Mul; break;
        case Op::MulMul: fb_op = minja::fbs::BinaryOpType::MulMul; break;
        case Op::Div: fb_op = minja::fbs::BinaryOpType::Div; break;
        case Op::DivDiv: fb_op = minja::fbs::BinaryOpType::DivDiv; break;
        case Op::Mod: fb_op = minja::fbs::BinaryOpType::Mod; break;
        case Op::Eq: fb_op = minja::fbs::BinaryOpType::Eq; break;
        case Op::Ne: fb_op = minja::fbs::BinaryOpType::Ne; break;
        case Op::Lt: fb_op = minja::fbs::BinaryOpType::Lt; break;
        case Op::Gt: fb_op = minja::fbs::BinaryOpType::Gt; break;
        case Op::Le: fb_op = minja::fbs::BinaryOpType::Le; break;
        case Op::Ge: fb_op = minja::fbs::BinaryOpType::Ge; break;
        case Op::And: fb_op = minja::fbs::BinaryOpType::And; break;
        case Op::Or: fb_op = minja::fbs::BinaryOpType::Or; break;
        case Op::In: fb_op = minja::fbs::BinaryOpType::In; break;
        case Op::NotIn: fb_op = minja::fbs::BinaryOpType::NotIn; break;
        case Op::Is: fb_op = minja::fbs::BinaryOpType::Is; break;
        case Op::IsNot: fb_op = minja::fbs::BinaryOpType::IsNot; break;
        default: throw std::runtime_error("Unknown BinaryOpType");
    }
    auto binary_data_offset = minja::fbs::CreateBinaryOpExprData(builder, left_offset, right_offset, fb_op);

    minja::fbs::ExpressionBuilder eb(builder);
    eb.add_location(loc_offset);
    eb.add_expression_type(minja::fbs::ExpressionType::Type_Binary);
    eb.add_expression_data_type(minja::fbs::ExpressionData::BinaryOpExprData);
    eb.add_expression_data(binary_data_offset.Union());
    return eb.Finish();
}

flatbuffers::Offset<minja::fbs::Expression> MethodCallExpr::serialize(flatbuffers::FlatBufferBuilder& builder) const {
    auto loc_offset = location.serialize(builder);
    auto object_offset = object ? object->serialize(builder) : flatbuffers::Offset<minja::fbs::Expression>();
    auto method_name_offset = method ? builder.CreateString(method->get_name()) : builder.CreateString("");
    auto args_offset = args.serialize(builder); 
    auto mc_data_offset = minja::fbs::CreateMethodCallExprData(builder, object_offset, method_name_offset, args_offset);

    minja::fbs::ExpressionBuilder eb(builder);
    eb.add_location(loc_offset);
    eb.add_expression_type(minja::fbs::ExpressionType::Type_MethodCall);
    eb.add_expression_data_type(minja::fbs::ExpressionData::MethodCallExprData);
    eb.add_expression_data(mc_data_offset.Union());
    return eb.Finish();
}

flatbuffers::Offset<minja::fbs::Expression> CallExpr::serialize(flatbuffers::FlatBufferBuilder& builder) const {
    auto loc_offset = location.serialize(builder);
    auto object_offset = object ? object->serialize(builder) : flatbuffers::Offset<minja::fbs::Expression>();
    auto args_offset = args.serialize(builder); 
    auto call_data_offset = minja::fbs::CreateCallExprData(builder, object_offset, args_offset);

    minja::fbs::ExpressionBuilder eb(builder);
    eb.add_location(loc_offset);
    eb.add_expression_type(minja::fbs::ExpressionType::Type_Call);
    eb.add_expression_data_type(minja::fbs::ExpressionData::CallExprData);
    eb.add_expression_data(call_data_offset.Union());
    return eb.Finish();
}

flatbuffers::Offset<minja::fbs::Expression> FilterExpr::serialize(flatbuffers::FlatBufferBuilder& builder) const {
    auto loc_offset = location.serialize(builder);
    flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<minja::fbs::Expression>>> parts_offset;
    if (!parts.empty()) {
        std::vector<flatbuffers::Offset<minja::fbs::Expression>> parts_vec;
        parts_vec.reserve(parts.size());
        for (const auto& part_expr : parts) {
            if(part_expr) parts_vec.push_back(part_expr->serialize(builder));
        }
        if(!parts_vec.empty()) parts_offset = builder.CreateVector(parts_vec);
    }
    auto filter_data_offset = minja::fbs::CreateFilterExprData(builder, parts_offset);

    minja::fbs::ExpressionBuilder eb(builder);
    eb.add_location(loc_offset);
    eb.add_expression_type(minja::fbs::ExpressionType::Type_Filter);
    eb.add_expression_data_type(minja::fbs::ExpressionData::FilterExprData);
    eb.add_expression_data(filter_data_offset.Union());
    return eb.Finish();
}

flatbuffers::Offset<minja::fbs::Expression> IfExpr::serialize(flatbuffers::FlatBufferBuilder& builder) const {
    auto loc_offset = location.serialize(builder);
    auto condition_offset = condition ? condition->serialize(builder) : flatbuffers::Offset<minja::fbs::Expression>();
    auto then_expr_offset = then_expr ? then_expr->serialize(builder) : flatbuffers::Offset<minja::fbs::Expression>();
    auto else_expr_offset = else_expr ? else_expr->serialize(builder) : flatbuffers::Offset<minja::fbs::Expression>();
    auto if_data_offset = minja::fbs::CreateIfExprData(builder, condition_offset, then_expr_offset, else_expr_offset);

    minja::fbs::ExpressionBuilder eb(builder);
    eb.add_location(loc_offset);
    eb.add_expression_type(minja::fbs::ExpressionType::Type_If);
    eb.add_expression_data_type(minja::fbs::ExpressionData::IfExprData);
    eb.add_expression_data(if_data_offset.Union());
    return eb.Finish();
}

// --- TemplateNode Subclasses ---

flatbuffers::Offset<minja::fbs::TemplateNode> SequenceNode::serialize(flatbuffers::FlatBufferBuilder& builder) const {
    auto loc_offset = location().serialize(builder);
    flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<minja::fbs::TemplateNode>>> children_offset;
    if (!children.empty()) {
        std::vector<flatbuffers::Offset<minja::fbs::TemplateNode>> children_vec;
        children_vec.reserve(children.size());
        for (const auto& child : children) {
            if (child) children_vec.push_back(child->serialize(builder));
        }
        if (!children_vec.empty()) children_offset = builder.CreateVector(children_vec);
    }
    auto seq_data_offset = minja::fbs::CreateSequenceNodeData(builder, children_offset);

    minja::fbs::TemplateNodeBuilder tnb(builder);
    tnb.add_location(loc_offset);
    tnb.add_node_type(minja::fbs::TemplateNodeType::SequenceNodeType);
    tnb.add_node_data_type(minja::fbs::TemplateNodeData::SequenceNodeData);
    tnb.add_node_data(seq_data_offset.Union());
    return tnb.Finish();
}

flatbuffers::Offset<minja::fbs::TemplateNode> TextNode::serialize(flatbuffers::FlatBufferBuilder& builder) const {
    auto loc_offset = location().serialize(builder);
    auto text_offset = builder.CreateString(text);
    auto text_data_offset = minja::fbs::CreateTextNodeData(builder, text_offset);

    minja::fbs::TemplateNodeBuilder tnb(builder);
    tnb.add_location(loc_offset);
    tnb.add_node_type(minja::fbs::TemplateNodeType::TextNodeType);
    tnb.add_node_data_type(minja::fbs::TemplateNodeData::TextNodeData);
    tnb.add_node_data(text_data_offset.Union());
    return tnb.Finish();
}

flatbuffers::Offset<minja::fbs::TemplateNode> ExpressionNode::serialize(flatbuffers::FlatBufferBuilder& builder) const {
    auto loc_offset = location().serialize(builder);
    auto expr_offset = expr ? expr->serialize(builder) : flatbuffers::Offset<minja::fbs::Expression>();
    auto expr_node_data_offset = minja::fbs::CreateExpressionNodeData(builder, expr_offset);

    minja::fbs::TemplateNodeBuilder tnb(builder);
    tnb.add_location(loc_offset);
    tnb.add_node_type(minja::fbs::TemplateNodeType::ExpressionNodeType);
    tnb.add_node_data_type(minja::fbs::TemplateNodeData::ExpressionNodeData);
    tnb.add_node_data(expr_node_data_offset.Union());
    return tnb.Finish();
}

flatbuffers::Offset<minja::fbs::TemplateNode> IfNode::serialize(flatbuffers::FlatBufferBuilder& builder) const {
    auto loc_offset = location().serialize(builder);
    flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<minja::fbs::IfNodeCascadeItemData>>> cascade_offset;
    if (!cascade.empty()) {
        std::vector<flatbuffers::Offset<minja::fbs::IfNodeCascadeItemData>> cascade_vec;
        cascade_vec.reserve(cascade.size());
        for (const auto& item : cascade) {
            auto cond_offset = item.first ? item.first->serialize(builder) : flatbuffers::Offset<minja::fbs::Expression>();
            auto body_offset = item.second ? item.second->serialize(builder) : flatbuffers::Offset<minja::fbs::TemplateNode>();
            cascade_vec.push_back(minja::fbs::CreateIfNodeCascadeItemData(builder, cond_offset, body_offset));
        }
        if (!cascade_vec.empty()) cascade_offset = builder.CreateVector(cascade_vec);
    }
    auto if_data_offset = minja::fbs::CreateIfNodeData(builder, cascade_offset);

    minja::fbs::TemplateNodeBuilder tnb(builder);
    tnb.add_location(loc_offset);
    tnb.add_node_type(minja::fbs::TemplateNodeType::IfNodeType);
    tnb.add_node_data_type(minja::fbs::TemplateNodeData::IfNodeData);
    tnb.add_node_data(if_data_offset.Union());
    return tnb.Finish();
}

flatbuffers::Offset<minja::fbs::TemplateNode> LoopControlNode::serialize(flatbuffers::FlatBufferBuilder& builder) const {
    auto loc_offset = location().serialize(builder);
    minja::fbs::LoopControlTypeFb fb_control_type;
    switch (control_type_) {
        case LoopControlType::Normal: fb_control_type = minja::fbs::LoopControlTypeFb::Normal; break;
        case LoopControlType::Break: fb_control_type = minja::fbs::LoopControlTypeFb::Break; break;
        case LoopControlType::Continue: fb_control_type = minja::fbs::LoopControlTypeFb::Continue; break;
        default: throw std::runtime_error("Unknown LoopControlType");
    }
    auto lc_data_offset = minja::fbs::CreateLoopControlNodeData(builder, fb_control_type);

    minja::fbs::TemplateNodeBuilder tnb(builder);
    tnb.add_location(loc_offset);
    tnb.add_node_type(minja::fbs::TemplateNodeType::LoopControlNodeType);
    tnb.add_node_data_type(minja::fbs::TemplateNodeData::LoopControlNodeData);
    tnb.add_node_data(lc_data_offset.Union());
    return tnb.Finish();
}

flatbuffers::Offset<minja::fbs::TemplateNode> ForNode::serialize(flatbuffers::FlatBufferBuilder& builder) const {
    auto loc_offset = location().serialize(builder);
    
    flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<flatbuffers::String>>> var_names_offset;
    if (!var_names.empty()) {
        std::vector<flatbuffers::Offset<flatbuffers::String>> names_vec;
        names_vec.reserve(var_names.size());
        for (const auto& name : var_names) {
            names_vec.push_back(builder.CreateString(name));
        }
        var_names_offset = builder.CreateVector(names_vec);
    }

    auto iterable_offset = iterable ? iterable->serialize(builder) : flatbuffers::Offset<minja::fbs::Expression>();
    auto condition_offset = condition ? condition->serialize(builder) : flatbuffers::Offset<minja::fbs::Expression>();
    auto body_offset = body ? body->serialize(builder) : flatbuffers::Offset<minja::fbs::TemplateNode>();
    auto else_body_offset = else_body ? else_body->serialize(builder) : flatbuffers::Offset<minja::fbs::TemplateNode>();

    auto for_data_offset = minja::fbs::CreateForNodeData(builder, var_names_offset, iterable_offset, condition_offset, body_offset, recursive, else_body_offset);

    minja::fbs::TemplateNodeBuilder tnb(builder);
    tnb.add_location(loc_offset);
    tnb.add_node_type(minja::fbs::TemplateNodeType::ForNodeType);
    tnb.add_node_data_type(minja::fbs::TemplateNodeData::ForNodeData);
    tnb.add_node_data(for_data_offset.Union());
    return tnb.Finish();
}

flatbuffers::Offset<minja::fbs::TemplateNode> MacroNode::serialize(flatbuffers::FlatBufferBuilder& builder) const {
    auto loc_offset = location().serialize(builder);
    auto name_offset = name ? builder.CreateString(name->get_name()) : builder.CreateString("");
    
    flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<minja::fbs::ParameterData>>> params_offset;
    if (!params.empty()) {
        std::vector<flatbuffers::Offset<minja::fbs::ParameterData>> params_vec;
        params_vec.reserve(params.size());
        for (const auto& param_pair : params) {
            auto param_name_offset = builder.CreateString(param_pair.first);
            auto default_val_offset = param_pair.second ? param_pair.second->serialize(builder) : flatbuffers::Offset<minja::fbs::Expression>();
            params_vec.push_back(minja::fbs::CreateParameterData(builder, param_name_offset, default_val_offset));
        }
        params_offset = builder.CreateVector(params_vec);
    }

    auto body_offset = body ? body->serialize(builder) : flatbuffers::Offset<minja::fbs::TemplateNode>();
    auto macro_data_offset = minja::fbs::CreateMacroNodeData(builder, name_offset, params_offset, body_offset);

    minja::fbs::TemplateNodeBuilder tnb(builder);
    tnb.add_location(loc_offset);
    tnb.add_node_type(minja::fbs::TemplateNodeType::MacroNodeType);
    tnb.add_node_data_type(minja::fbs::TemplateNodeData::MacroNodeData);
    tnb.add_node_data(macro_data_offset.Union());
    return tnb.Finish();
}

flatbuffers::Offset<minja::fbs::TemplateNode> FilterNode::serialize(flatbuffers::FlatBufferBuilder& builder) const {
    auto loc_offset = location().serialize(builder);
    auto filter_expr_offset = filter ? filter->serialize(builder) : flatbuffers::Offset<minja::fbs::Expression>();
    auto body_offset = body ? body->serialize(builder) : flatbuffers::Offset<minja::fbs::TemplateNode>();
    auto filter_data_offset = minja::fbs::CreateFilterNodeData(builder, filter_expr_offset, body_offset);

    minja::fbs::TemplateNodeBuilder tnb(builder);
    tnb.add_location(loc_offset);
    tnb.add_node_type(minja::fbs::TemplateNodeType::FilterNodeType);
    tnb.add_node_data_type(minja::fbs::TemplateNodeData::FilterNodeData);
    tnb.add_node_data(filter_data_offset.Union());
    return tnb.Finish();
}

flatbuffers::Offset<minja::fbs::TemplateNode> SetNode::serialize(flatbuffers::FlatBufferBuilder& builder) const {
    auto loc_offset = location().serialize(builder);
    auto ns_offset = builder.CreateString(ns);
    
    flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<flatbuffers::String>>> var_names_offset;
    if (!var_names.empty()) {
        std::vector<flatbuffers::Offset<flatbuffers::String>> names_vec;
        names_vec.reserve(var_names.size());
        for (const auto& name_str : var_names) {
            names_vec.push_back(builder.CreateString(name_str));
        }
        var_names_offset = builder.CreateVector(names_vec);
    }

    auto value_offset = value ? value->serialize(builder) : flatbuffers::Offset<minja::fbs::Expression>();
    auto set_data_offset = minja::fbs::CreateSetNodeData(builder, ns_offset, var_names_offset, value_offset);

    minja::fbs::TemplateNodeBuilder tnb(builder);
    tnb.add_location(loc_offset);
    tnb.add_node_type(minja::fbs::TemplateNodeType::SetNodeType);
    tnb.add_node_data_type(minja::fbs::TemplateNodeData::SetNodeData);
    tnb.add_node_data(set_data_offset.Union());
    return tnb.Finish();
}

flatbuffers::Offset<minja::fbs::TemplateNode> SetTemplateNode::serialize(flatbuffers::FlatBufferBuilder& builder) const {
    auto loc_offset = location().serialize(builder);
    auto name_offset = builder.CreateString(name);
    auto template_value_offset = template_value ? template_value->serialize(builder) : flatbuffers::Offset<minja::fbs::TemplateNode>();
    auto set_template_data_offset = minja::fbs::CreateSetTemplateNodeData(builder, name_offset, template_value_offset);

    minja::fbs::TemplateNodeBuilder tnb(builder);
    tnb.add_location(loc_offset);
    tnb.add_node_type(minja::fbs::TemplateNodeType::SetTemplateNodeType);
    tnb.add_node_data_type(minja::fbs::TemplateNodeData::SetTemplateNodeData);
    tnb.add_node_data(set_template_data_offset.Union());
    return tnb.Finish();
}

} // namespace minja
