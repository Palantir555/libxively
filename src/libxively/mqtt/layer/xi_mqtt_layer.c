// Copyright (c) 2003-2014, LogMeIn, Inc. All rights reserved.
// This is part of Xively C library, it is under the BSD 3-Clause license.

#include "xi_coroutine.h"
#include "xi_mqtt_layer.h"

#include "message.h"
#include "serialiser.h"
#include "parser.h"

#ifdef __cplusplus
extern "C" {
#endif

layer_state_t xi_mqtt_layer_data_ready(
      layer_connectivity_t* context
    , const void* data
    , const layer_hint_t hint )
{
    XI_UNUSED( hint );

    const mqtt_message_t* msg = ( const mqtt_message_t* ) data;

    mqtt_serialiser_t serializer;
    mqtt_serialiser_init( & serializer );

    int len = mqtt_serialiser_size( &serializer, msg );

    uint8_t buffer[ 256 ];
    data_descriptor_t data_descriptor = { ( char* ) buffer, len, len, 0 };

    mqtt_serialiser_rc_t rc = mqtt_serialiser_write( &serializer, msg, buffer, len );

    if( rc == MQTT_SERIALISER_RC_ERROR )
    {
        return LAYER_STATE_ERROR;
    }

    layer_state_t state = CALL_ON_PREV_DATA_READY( context->self, ( void* ) &data_descriptor, LAYER_HINT_NONE );

    return state;
}

layer_state_t xi_mqtt_layer_on_data_ready(
      layer_connectivity_t* context
    , const void* data
    , const layer_hint_t hint )
{
    XI_UNUSED( hint );

    // must survive the yield
    static uint16_t cs          = 0;
    static layer_state_t state  = LAYER_STATE_OK; //

    // tmp variables for
    const const_data_descriptor_t* data_descriptor = ( const const_data_descriptor_t* ) data;
    xi_mqtt_layer_data_t* layer_data = ( xi_mqtt_layer_data_t* ) context->self->user_data;

    BEGIN_CORO( cs )

    mqtt_parser_init( &layer_data->parser );

    do
    {
        state = mqtt_parser_execute(
              &layer_data->parser
            , &layer_data->msg
            , ( const uint8_t* ) data_descriptor->data_ptr
            , data_descriptor->real_size, 0 );

        YIELD_UNTIL( cs, ( state == LAYER_STATE_WANT_READ ), LAYER_STATE_WANT_READ );
    } while( state == LAYER_STATE_WANT_READ );

    EXIT( cs, LAYER_STATE_OK );

    END_CORO()
}

layer_state_t xi_mqtt_layer_close(
    layer_connectivity_t* context )
{
    mqtt_message_t message;
    memset( &message, 0, sizeof( mqtt_message_t ) );
    message.common.common_u.common_bits.type = MQTT_TYPE_DISCONNECT;

    CALL_ON_SELF_DATA_READY( context->self, &message, LAYER_HINT_NONE );

    return CALL_ON_PREV_CLOSE( context->self );
}

layer_state_t xi_mqtt_layer_on_close(
    layer_connectivity_t* context )
{
    XI_UNUSED( context );
    return LAYER_STATE_OK;
}

#ifdef __cplusplus
}
#endif